# build v0~v10 topk code

VERSION=$1
DEBUG=$2

ROOT_DIR=$(cd $(dirname $0); pwd)

if [ -z "$VERSION" ]; then
    topk_file="topk.cu"
elif (( VERSION == 0 )); then
    topk_file="topk_v0_base.cu"
elif (( VERSION == 1 )); then
    topk_file="topk_v1_multi_threads.cu"
elif (( VERSION == 2 )); then
    topk_file="topk_v2_bitset.cu"
elif (( VERSION == 3 )); then
    topk_file="topk_v3_faster_topk.cu"
elif (( VERSION == 4 )); then
    topk_file="topk_v4_batch.cu"
elif (( VERSION == 5 )); then
    topk_file="topk_v5_shared_memory.cu"
elif (( VERSION == 6 )); then
    topk_file="topk_v6_query_sort.cu"
elif (( VERSION == 7 )); then
    topk_file="topk_v7_popc.cu"
elif (( VERSION == 8 )); then
    topk_file="topk_v8_thresh.cu"
elif (( VERSION == 9 )); then
    topk_file="topk_v9_batch_thresh.cu"
elif (( VERSION == 10 )); then
    topk_file="topk_v10_cuda_init.cu"
else
    echo "VERSION not support"
    exit 1
fi

flags="-G"
if [ -z "$DEBUG" ]; then
    flags="-O3"
elif (( DEBUG == 0 )); then
    flags="-O3"
fi

echo "current topk file: ${topk_file}"

topk_path="$ROOT_DIR/src/$topk_file"

nvcc $ROOT_DIR/src/main.cpp  $topk_path -o $ROOT_DIR/bin/query_doc_scoring_$VERSION  -I $ROOT_DIR/src -L/usr/local/cuda/lib64  $flags -lcudart -lcuda

echo "build success"