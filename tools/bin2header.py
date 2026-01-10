#!/usr/bin/env python3
import sys

if len(sys.argv) != 4:
    print("Usage: python bin2header.py <input.tim> <output.h> <array_name>")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]
array_name = sys.argv[3]

with open(input_file, 'rb') as f:
    data = f.read()

header = f'const unsigned char {array_name}[{len(data)}] = {{\n'
lines = []
for i in range(0, len(data), 16):
    chunk = data[i:i+16]
    line = '    ' + ', '.join(f'0x{b:02X}' for b in chunk)
    lines.append(line)
header += ',\n'.join(lines) + '\n};\n'

with open(output_file, 'w') as f:
    f.write(header)

print(f'Header regenerated: {len(data)} bytes')
