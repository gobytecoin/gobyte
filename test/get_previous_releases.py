#!/usr/bin/env python3
#
# Copyright (c) 2018-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Download or build previous releases.
# Needs curl and tar to download a release, or the build dependencies when
# building a release.

import argparse
import contextlib
from fnmatch import fnmatch
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import hashlib

SHA256_SUMS = {
    "ef75bce767dd585303874f8c53c95731f40378326c82a420c7895cb43f3ffdc0": {"tag": "v0.16.2.2", "tarball": "gobyte-0.16.2.2-arm64-linux-gnu.tar.xz"},
    "7e035f39e3a52ce02efa72638ebb154a296021621fccc6e47ee0ef164bedbe98": {"tag": "v0.16.2.2", "tarball": "GoByte-Core.dmg"},
    "2375d00eb6cb422ca7fbff3fa833150df55457d45913a1f225f57087fd85933c": {"tag": "v0.16.2.2", "tarball": "gobyte-0.16.2.2-win64.zip"},
    "519b271482f2526172ac3d3249fbc7c0ae311ec478e918a35e2bb6ea4cf67e5b": {"tag": "v0.16.2.2", "tarball": "gobyte-0.16.2.2-x86_64-linux-gnu.tar.xz"},
    #
    "4735be7e362058a2ef2a80e7cc3511766b8f40133a6af6f185974ddee6e5e66b": {"tag": "v0.16.2.1", "tarball": "gobytecore-0.16.2-aarch64-linux-gnu.tar.gz"},
    "d5decf9539f31a2c3af8c3d20a832858981d0be3a66648c2ece0c4ff6d27a139": {"tag": "v0.16.2.1", "tarball": "gobytecore-0.16.2-arm-linux-gnueabihf.tar.gz"},
    "dd907af326b0cfe604adae1ee82274a03b528f81a7e507da7a71c407ea44ca3b": {"tag": "v0.16.2.1", "tarball": "gobytecore-0.16.2-osx-unsigned.dmg"},
    "78ab9e3eccdd732d01f1455481338928068511af9d9e8ffa13fd93338fe6d19b": {"tag": "v0.16.2.1", "tarball": "gobytecore-0.16.2-osx64.tar.gz"},
    "3d957ba9db141e0274b2889d4094014ebdf237a4776a56b1b9ac707e4a46a76a": {"tag": "v0.16.2.1", "tarball": "gobytecore-0.16.2-win64.zip"},
    "acb1cf006a5a575598a6ee64c7a50ef342bb1c8d9f92c76cc6387eb6e0cc0f1a": {"tag": "v0.16.2.1", "tarball": "gobytecore-0.16.2-x86_64-linux-gnu.tar.gz"},
}


@contextlib.contextmanager
def pushd(new_dir) -> None:
    previous_dir = os.getcwd()
    os.chdir(new_dir)
    try:
        yield
    finally:
        os.chdir(previous_dir)


def download_binary(tag, args) -> int:
    if Path(tag).is_dir():
        if not args.remove_dir:
            print('Using cached {}'.format(tag))
            return 0
        shutil.rmtree(tag)
    Path(tag).mkdir()
    bin_path = 'releases/download/v{}'.format(tag[1:])
    match = re.compile('v(.*)(rc[0-9]+)$').search(tag)
    if match:
        bin_path = 'releases/download/test.{}'.format(
            match.group(1), match.group(2))
    platform = args.platform
    if platform in ["x86_64-w64-mingw32"]:
        platform = "win64"
    elif tag < "v0.12.3":
        if platform in ["arm-linux-gnueabihf"]:
            platform = "RPi2"
        elif platform in ["x86_64-apple-darwin", "arm64-apple-darwin"]:
            print(f"Binaries not available for {tag} on {platform}")
            return 1
        elif platform in ["x86_64-linux-gnu"]:
            platform = "linux64"
    elif tag < "v20" and platform in ["x86_64-apple-darwin", "arm64-apple-darwin"]:
        platform = "osx64"
    tarball = 'gobytecore-{tag}-{platform}.tar.gz'.format(
        tag=tag[1:], platform=platform)
    tarballUrl = 'https://github.com/gobytecoin/gobyte/{bin_path}/{tarball}'.format(
        bin_path=bin_path, tarball=tarball)

    print('Fetching: {tarballUrl}'.format(tarballUrl=tarballUrl))

    header, status = subprocess.Popen(
        ['curl', '--head', tarballUrl], stdout=subprocess.PIPE).communicate()
    if re.search("404 Not Found", header.decode("utf-8")):
        print("Binary tag was not found")
        return 1

    curlCmds = [
        ['curl', '-L', '--remote-name', tarballUrl]
    ]

    for cmd in curlCmds:
        ret = subprocess.run(cmd).returncode
        if ret:
            return ret

    hasher = hashlib.sha256()
    with open(tarball, "rb") as afile:
        hasher.update(afile.read())
    tarballHash = hasher.hexdigest()

    if tarballHash not in SHA256_SUMS or SHA256_SUMS[tarballHash]['tarball'] != tarball:
        if tarball in [v['tarball'] for v in SHA256_SUMS.values()]:
            print("Checksum did not match")
            return 1

        print("Checksum for given version doesn't exist")
        return 1
    print("Checksum matched")

    # Extract tarball
    # special case for v17 and earlier: other name of version
    filename = tag[1:-2] if tag[1:3] == "0." else tag[1:]
    ret = subprocess.run(['tar', '-zxf', tarball, '-C', tag,
                          '--strip-components=1',
                          'gobytecore-{tag}'.format(tag=filename, platform=args.platform)]).returncode
    if ret != 0:
        print(f"Failed to extract the {tag} tarball")
        return ret

    Path(tarball).unlink()

    if tag >= "v19" and args.host == "arm64-apple-darwin":
        # Starting with v19 there are arm64 binaries for ARM (e.g. M1, M2) macs, but they have to be signed to run
        binary_path = f'{os.getcwd()}/{tag}/bin/'

        for arm_binary in os.listdir(binary_path):
            # Is it already signed?
            ret = subprocess.run(
                ['codesign', '-v', binary_path + arm_binary],
                stderr=subprocess.DEVNULL,  # Suppress expected stderr output
            ).returncode
            if ret == 1:
                # Have to self-sign the binary
                ret = subprocess.run(
                    ['codesign', '-s', '-', binary_path + arm_binary]
                ).returncode
                if ret != 0:
                    print(f"Failed to self-sign {tag} {arm_binary} arm64 binary")
                    return 1

                # Confirm success
                ret = subprocess.run(
                    ['codesign', '-v', binary_path + arm_binary]
                ).returncode
                if ret != 0:
                    print(f"Failed to verify the self-signed {tag} {arm_binary} arm64 binary")
                    return 1

    return 0


def build_release(tag, args) -> int:
    githubUrl = "https://github.com/gobytecoin/gobyte"
    if args.remove_dir:
        if Path(tag).is_dir():
            shutil.rmtree(tag)
    if not Path(tag).is_dir():
        # fetch new tags
        subprocess.run(
            ["git", "fetch", githubUrl, "--tags"])
        output = subprocess.check_output(['git', 'tag', '-l', tag])
        if not output:
            print('Tag {} not found'.format(tag))
            return 1
    ret = subprocess.run([
        'git', 'clone', f'--branch={tag}', '--depth=1', githubUrl, tag
    ]).returncode
    if ret:
        return ret
    with pushd(tag):
        host = args.host
        if args.depends:
            with pushd('depends'):
                ret = subprocess.run(['make', 'NO_QT=1']).returncode
                if ret:
                    return ret
                host = os.environ.get(
                    'HOST', subprocess.check_output(['./config.guess']))
        config_flags = '--prefix={pwd}/depends/{host} '.format(
            pwd=os.getcwd(),
            host=host) + args.config_flags
        cmds = [
            './autogen.sh',
            './configure {}'.format(config_flags),
            'make',
        ]
        for cmd in cmds:
            ret = subprocess.run(cmd.split()).returncode
            if ret:
                return ret
        # Move binaries, so they're in the same place as in the
        # release download
        Path('bin').mkdir(exist_ok=True)
        files = ['gobyted', 'gobyte-cli', 'gobyte-tx']
        for f in files:
            Path('src/'+f).rename('bin/'+f)
    return 0


def check_host(args) -> int:
    args.host = os.environ.get('HOST', subprocess.check_output(
        './depends/config.guess').decode())
    if args.download_binary:
        platforms = {
            'aarch64-*-linux*': 'aarch64-linux-gnu',
            'x86_64-*-linux*': 'x86_64-linux-gnu',
            'x86_64-apple-darwin*': 'x86_64-apple-darwin',
            'aarch64-apple-darwin*': 'arm64-apple-darwin',
        }
        args.platform = ''
        for pattern, target in platforms.items():
            if fnmatch(args.host, pattern):
                args.platform = target
        if not args.platform:
            print('Not sure which binary to download for {}'.format(args.host))
            return 1
    return 0


def main(args) -> int:
    Path(args.target_dir).mkdir(exist_ok=True, parents=True)
    print("Releases directory: {}".format(args.target_dir))
    ret = check_host(args)
    if ret:
        return ret
    if args.download_binary:
        with pushd(args.target_dir):
            for tag in args.tags:
                ret = download_binary(tag, args)
                if ret:
                    return ret
        return 0
    args.config_flags = os.environ.get('CONFIG_FLAGS', '')
    args.config_flags += ' --without-gui --disable-tests --disable-bench'
    with pushd(args.target_dir):
        for tag in args.tags:
            ret = build_release(tag, args)
            if ret:
                return ret
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        epilog='''
        HOST can be set to any of the `host-platform-triplet`s from
        depends/README.md for which a release exists.
        ''',
    )
    parser.add_argument('-r', '--remove-dir', action='store_true',
                        help='remove existing directory.')
    parser.add_argument('-d', '--depends', action='store_true',
                        help='use depends.')
    parser.add_argument('-b', '--download-binary', action='store_true',
                        help='download release binary.')
    parser.add_argument('-t', '--target-dir', action='store',
                        help='target directory.', default='releases')
    all_tags = sorted([*set([v['tag'] for v in SHA256_SUMS.values()])])
    parser.add_argument('tags', nargs='*', default=all_tags,
                        help='release tags. e.g.: v19.3.0 v19.0.0-rc.9 '
                        '(if not specified, the full list needed for '
                        'backwards compatibility tests will be used)'
                        )
    args = parser.parse_args()
    sys.exit(main(args))
