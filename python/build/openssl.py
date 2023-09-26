import subprocess
from typing import Optional, Sequence, Union

from build.makeproject import MakeProject
from .toolchain import AnyToolchain

class OpenSSLProject(MakeProject):
    def __init__(self, url: Union[str, Sequence[str]], md5: str, installed: str,
                 **kwargs):
        MakeProject.__init__(self, url, md5, installed, install_target='install_dev', **kwargs)

    def get_make_args(self, toolchain: AnyToolchain) -> list[str]:
        return MakeProject.get_make_args(self, toolchain) + [
            'CC=' + toolchain.cc,
            'CFLAGS=' + toolchain.cflags,
            'CPPFLAGS=' + toolchain.cppflags,
            'AR=' + toolchain.ar,
            'RANLIB=' + toolchain.ranlib,
            'build_libs',
        ]

    def get_make_install_args(self, toolchain: AnyToolchain) -> list[str]:
        # OpenSSL's Makefile runs "ranlib" during installation
        return MakeProject.get_make_install_args(self, toolchain) + [
            'RANLIB=' + toolchain.ranlib,
        ]

    def _build(self, toolchain: AnyToolchain) -> None:
        src = self.unpack(toolchain, out_of_tree=False)

        # OpenSSL has a weird target architecture scheme with lots of
        # hard-coded architectures; this table translates between our
        # "toolchain_arch" (HOST_TRIPLET) and the OpenSSL target
        openssl_archs = {
            # not using "android-*" because those OpenSSL targets want
            # to know where the SDK is, but our own build scripts
            # prepared everything already to look like a regular Linux
            # build
            'arm-linux-androideabi': 'linux-generic32',
            'aarch64-linux-android': 'linux-aarch64',
            'i686-linux-android': 'linux-x86-clang',
            'x86_64-linux-android': 'linux-x86_64-clang',

            # generic Linux
            'arm-linux-gnueabihf': 'linux-generic32',

            # Windows
            'i686-w64-mingw32': 'mingw',
            'x86_64-w64-mingw32': 'mingw64',

            # Apple
            'x86_64-apple-darwin': 'darwin64-x86_64-cc',
            'aarch64-apple-darwin': 'darwin64-arm64-cc',
        }

        openssl_arch = openssl_archs[toolchain.arch]

        subprocess.check_call(['./Configure',
                               'no-shared',
                               'no-module', 'no-engine', 'no-static-engine',
                               'no-async',
                               'no-tests',
                               'no-asm', # "asm" causes build failures on Windows
                               openssl_arch,
                               '--prefix=' + toolchain.install_prefix],
                              cwd=src, env=toolchain.env)
        self.build_make(toolchain, src)
