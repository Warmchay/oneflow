#!/usr/bin/env python2
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

import asyncio
import argparse
import subprocess
import os


def split_and_print(prefix, text):
    lines = text.decode().splitlines(keepends=True)
    prefixed = ""
    for l in lines:
        prefixed += f"{prefix} {l.strip()}"
    if l.strip():
        print(prefixed, flush=True)


async def handle_stream(stream, cb):
    while True:
        line = await stream.readline()
        if line:
            cb(line)
        else:
            break


async def run_command(cmd=None, dry=False, name=None):
    if dry:
        print(f"[dry] {cmd}")
        return 0
    process = await asyncio.create_subprocess_shell(
        cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE,
    )
    l = lambda x: split_and_print(f"[{name}]" if name else "", x)
    await asyncio.gather(
        handle_stream(process.stdout, l), handle_stream(process.stderr, l),
    )
    await process.wait()
    return process.returncode


def download(build_dir, dry=False):
    urls = [
        "https://github.com/Oneflow-Inc/llvm-project/releases/download/latest/clang-tidy-489012f-x86_64.AppImage"
        if os.getenv("CI")
        else "https://oneflow-static.oss-cn-beijing.aliyuncs.com/bin/clang-tidy/linux-x86_64/clang-tidy.AppImage",
        "https://raw.githubusercontent.com/oneflow-inc/llvm-project/maybe/clang-tools-extra/clang-tidy/tool/clang-tidy-diff.py",
    ]
    dst_dir = f"{build_dir}/cache/bin"
    dst = [f"{dst_dir}/clang-tidy", f"{dst_dir}/clang-tidy-diff.py"]
    if dry:
        if os.path.isfile(dst[0]) and os.path.isfile(dst[1]):
            return dst
        else:
            None
    else:
        assert subprocess.call(f"mkdir -p {dst_dir}", shell=True) == 0
        for i, _dst in enumerate(dst):
            assert subprocess.call(f"curl -L {urls[i]} -o {_dst}", shell=True) == 0
            assert subprocess.call(f"chmod +x {_dst}", shell=True) == 0
        return dst


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Runs clang-tidy on all of the source files."
    )
    parser.add_argument(
        "--build_dir", required=True,
    )
    args = parser.parse_args()
    loop = asyncio.get_event_loop()
    downloaded = download(args.build_dir, dry=True)
    if downloaded is None:
        downloaded = download(args.build_dir)
    promises = [
        run_command(
            f"cd .. && git diff -U0 master | {downloaded[1]} -clang-tidy-binary {downloaded[0]} -path {args.build_dir} -j $(nproc) -p1 -allow-enabling-alpha-checkers -extra-arg=-Xclang -extra-arg=-analyzer-config -extra-arg=-Xclang -extra-arg=aggressive-binary-operation-simplification=true"
        )
    ]
    loop.run_until_complete(asyncio.gather(*promises))
