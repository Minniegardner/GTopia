import os
import sys

def hash(s: str) -> int:
    h = 0x12668559
    
    for c in s.encode():
        h ^= c
        h = (h * 0x5bd1e995) & 0xFFFFFFFF
        h ^= (h >> 15)
        h &= 0xFFFFFFFF

    return h

if len(sys.argv) < 2:
    print("Usage: python3 generate_enums.py <file_path> (prefix)")
    exit(1)
else:
    filePath = sys.argv[1]
    prefix = sys.argv[2]

if not os.path.isfile(filePath):
    print(f"{filePath} is not exists")
    exit(1)

with open(filePath) as f:
    keys = [line.strip() for line in f if line.strip() and not line.strip().startswith("#")]

for k in keys:
    print(f"{prefix}{k.upper()},")