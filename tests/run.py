# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ALOLAN
"""Windows LLVM 无 CRT 主机测试。示例：python tests/run.py --clang PATH/clang.exe"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--clang', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
clang = Path(args.clang).resolve()
manifest = json.loads((root / 'User/ThirdParty/lwrb/provenance.json').read_text(encoding='utf-8'))
for name, digest in manifest['sha256'].items():
    assert hashlib.sha256((root / 'User/ThirdParty/lwrb' / name).read_bytes()).hexdigest() == digest, name
print('PASS upstream SHA-256', flush=True)
sources = ['tests/test_ringbuf.c', 'tests/runtime.c', 'User/Utils/utils_ringbuf.c',
           'User/ThirdParty/lwrb/src/lwrb/lwrb.c', 'examples/ringbuf_example.c']
common = ['-std=c99', '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
          '-DLWRB_DISABLE_ATOMIC', '-Itests/stubs', '-IUser/Utils', '-Iexamples',
          '-IUser/ThirdParty/lwrb/src/include', '-Wall', '-Wextra']
tests = ['test_fifo', 'test_parameters', 'test_statistics', 'test_model', 'test_example',
         'test_peek_skip', 'test_write_exact', 'test_storage_limit', 'test_read_exact']
for opt in ['0', '2']:
    out = root / 'build' / ('host-O' + opt)
    out.mkdir(parents=True, exist_ok=True)
    objects = []
    for index, source in enumerate(sources):
        obj = out / f'{index}.obj'
        strict = [] if 'ThirdParty' in source else ['-Werror', '-Wconversion', '-Wsign-conversion']
        subprocess.run([str(clang), *common, *strict, '-O' + opt, '-c', source, '-o', str(obj)],
                       cwd=root, check=True)
        objects.append(str(obj))
    dll = out / 'tests.dll'
    subprocess.run([str(clang.with_name('lld.exe')), '-flavor', 'link', '/dll', '/noentry',
                    '/nodefaultlib', f'/out:{dll}', *objects], check=True)
    library = ctypes.CDLL(str(dll))
    for name in tests:
        function = getattr(library, name)
        function.restype = ctypes.c_int
        result = function()
        if result != 0:
            raise AssertionError(f'O{opt} {name}: test source line {result}')
        print(f'PASS O{opt} {name}', flush=True)
out = root / 'build' / 'compile32'
out.mkdir(exist_ok=True)
for index, source in enumerate(sources):
    subprocess.run([str(clang), *common, '-m32', '-c', source, '-o', str(out / f'{index}.obj')],
                   cwd=root, check=True)
print('PASS 32-bit compile (not executed)', flush=True)
