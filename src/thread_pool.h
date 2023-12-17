#pragma once

#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <cassert>

/// @brief 执行空指令
inline int do_some_nops() {
    asm volatile (
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
    );
    return 64;
}

/// 最大忙等待nop指令执行数
constexpr int max_busy_wait_nops = 4 * 1000 * 1000;

/// @brief 等待原子变量var的新值
/// @param var 原子变量
/// @param init_value var的初始值
/// @param cond 条件变量
/// @param mutex 互斥锁
/// @return var的新值
template <typename T>
T wait_for_change(
        std::atomic<T>* var,
        T init_value,
        std::condition_variable* cond,
        std::mutex* mutex) {
    int nops = 0;
    T new_value = var->load(std::memory_order_acquire);
    // 拿到新值直接返回
    if (new_value != init_value) {
        return new_value;
    }

    // 空转并尝试拿到新值
    while (nops < max_busy_wait_nops) {
        nops += do_some_nops();
        new_value = var->load(std::memory_order_acquire);
        if (new_value != init_value) {
            return new_value;
        }
    }

    // 仍然没有拿到新值
    std::unique_lock<std::mutex> g(*mutex);
    new_value = var->load(std::memory_order_acquire);
    // 使用条件变量等待新值
    cond->wait(g, [&]() {
        new_value = var->load(std::memory_order_acquire);
        return new_value != init_value;
    });
    return new_value;
}

class Barrier {
public:

    Barrier() : _count(0) {}

    /// @brief 重置计数器
    void reset(std::size_t count) {
        std::size_t old_count = _count.load(std::memory_order_relaxed);
        assert(old_count == 0);
        (void)old_count;
        _count.store(count, std::memory_order_release);
    }

    /// @brief 计数器减1
    /// @return 计数器是否为0
    bool decrement() {
        std::size_t old_count_value = _count.fetch_sub(1, std::memory_order_acq_rel);
        assert(old_count_value > 0);
        std::size_t count_value = old_count_value - 1;
        return count_value == 0;
    }

    /// @brief 等待计数器归0
    void wait() {
        int nops = 0;
        // 指导计数器为0跳出循环返回
        while (_count.load(std::memory_order_acquire)) {
            nops += do_some_nops();
            // 超过最大忙等待nop执行数则休眠线程
            if (nops > max_busy_wait_nops) {
                nops = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

private:
    std::atomic<std::size_t> _count;
};

struct Task {
    virtual void run() = 0;
    virtual ~Task() = default;
};

class Worker {
public:
    enum class State :uint8_t {
        Start,
        Ready,
        Busy,
        Exit
    };

    explicit Worker(Barrier* barrier)
        : _task(nullptr),
          _state(State::Start),
          _barrier(barrier) {
        // 此时线程已启动并运行
        _thread.reset(new std::thread([this]() { this->run();}));
    }

    ~Worker() {
        change_state(State::Exit);
        _thread->join();
    }

    /// @brief 改变线程状态(可能由非Worker线程调用)
    void change_state(State new_state) {
        std::lock_guard<std::mutex> g(_state_mutex);
        assert(new_state != _state.load(std::memory_order_relaxed));
        switch (_state.load(std::memory_order_relaxed)) {
        case State::Start:
            assert(new_state == State::Ready);
            break;
        case State::Ready:
            assert(new_state == State::Busy || new_state == State::Exit);
            break;
        case State::Busy:
            assert(new_state == State::Ready || new_state == State::Exit);
            break;
        default:
            break;
        }
        _state.store(new_state, std::memory_order_relaxed);
        // 启动一个线程
        _state_cond.notify_one();
        // 转为就绪后屏障计数器减1
        if (new_state == State::Ready) {
            _barrier->decrement();
        }
    }

    /// @brief 由Worker线程调用
    void  run() {
        change_state(State::Ready);
        while (true) {
            // 等待就绪线程的状态改变
            State new_state = wait_for_change(&_state, State::Ready, &_state_cond, &_state_mutex);
            switch (new_state) {
            case State::Busy:
                (*_task).run(); // 运行任务
                _task = nullptr;
                // 运行完后转为就绪线程继续等待新任务
                change_state(State::Ready);
                break;
            case State::Exit:   // 结束Worker线程
                return;
            default:
                break;
            }
        }
    }

    /// @brief 运行指定任务
    void run_task(Task* task) {
        _task = task;
        assert(_state.load(std::memory_order_acquire) == State::Ready);
        // 转换Worker线程为忙碌状态,即执行task任务
        change_state(State::Busy);
    }

private:
    std::unique_ptr<std::thread> _thread;
    Task* _task;

    std::condition_variable _state_cond;
    std::mutex _state_mutex;

    std::atomic<State> _state;
    Barrier* const _barrier;
};

class ThreadPool {
public:
    ~ThreadPool() {
        for (auto* w : _workers) {
            delete w;
        }
    }

    size_t num_threads() {
        return _workers.size();
    }

    /// @brief 设置线程池的工作线程数
    /// @param nt 线程数
    void set_num_threads(size_t nt) {
        assert(nt >= 1);
        size_t total_worker = nt;
        if (_workers.size() >= total_worker) {
            return;
        }
        _barrier.reset(total_worker - _workers.size());
        while (_workers.size() < total_worker) {
            _workers.push_back(new Worker(&_barrier));
        }
    }

    /// @brief 多线程运行任务
    /// @param tasks 任务列表
    void run_task(const std::vector<Task*>& tasks) {
        assert(tasks.size() >= 1);
        set_num_threads(tasks.size());
        std::size_t workers_count = tasks.size();
        _barrier.reset(workers_count);
        // 让线程池中的Worker线程执行对应任务
        for (size_t i = 0; i < workers_count; ++i) {
            _workers[i]->run_task(tasks[i]);
        }
    }

    /// @brief 等待线程池中Worker线程均执行完毕
    void wait() {
        _barrier.wait();
    }

private:
    std::vector<Worker*> _workers;
    Barrier _barrier;
};