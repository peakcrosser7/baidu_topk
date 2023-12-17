import random

def generate_sequence(length):
    sequence = random.sample(range(1, 50000), length)
    sequence.sort()
    return sequence

def generate_files(n, path):
    for i in range(1, n+1):
        file_path = path + f"query{i}.txt"
        sequence_length = random.randint(1, 128)
        sequence = generate_sequence(sequence_length)
        sequence_str = ','.join(map(str, sequence))
        with open(file_path, 'w') as file:
            file.write(sequence_str)

# generate n random query

n = 2000
path = "./translate/querys2000/"
generate_files(n, path)