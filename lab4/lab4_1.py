from pwn import *
import random
z = random.randbytes(16)
context.arch, context.bits, _ = 'amd64', 64, print(disasm(z))
context.arch, context.bits, _ = 'i386', 32, print(disasm(z))