#!/usr/bin/env python3
"""Generate integrity signature for Aris.exe (SHA-512 truncated to 32 bytes).

Usage: python gen_sig.py <exe_path> <sig_output_path>

Writes raw 32 bytes (first half of SHA-512) to the .sig file.
Must match tweetnacl crypto_hash() truncation in WindowsMain.cpp.
"""
import hashlib
import sys

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <exe_path> <sig_output_path>", file=sys.stderr)
    sys.exit(1)

exe_path, sig_path = sys.argv[1], sys.argv[2]

with open(exe_path, "rb") as f:
    data = f.read()

h = hashlib.sha512(data).digest()[:32]  # first 32 bytes of SHA-512

with open(sig_path, "wb") as f:
    f.write(h)

print(f"  Signature: {h.hex()[:16]}... ({len(data)} bytes) -> {sig_path}")

