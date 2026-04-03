package=gmp
$(package)_version=6.1.2
$(package)_download_path=https://ftp.gnu.org/gnu/gmp/
$(package)_file_name=gmp-$($(package)_version).tar.bz2
$(package)_sha256_hash=5275bb04f4863a13516b2f39392ac5e272f5e1bb8057b18aec1c9b79d73d8fb2

define $(package)_set_vars
$(package)_config_opts=--enable-cxx --with-pic --disable-shared
$(package)_config_opts_x86_64_darwin=ABI=64
$(package)_cflags_armv7l_linux+=-march=armv7-a
# When cross-compiling for Darwin on Linux, GMP's ABI detection probe uses
# only $CC $CFLAGS (not $LDFLAGS).  Without these flags the probe invokes
# GNU ld which cannot process Darwin .tbd stubs, causing the link to fail.
# -fuse-ld=lld: selects LLD (which understands .tbd) for the probe link.
# -nostartfiles: skips crt1.o which is absent from the headers-only SDK.
$(package)_cflags_darwin+=-fuse-ld=lld -nostartfiles
$(package)_cxxflags_darwin+=-fuse-ld=lld -nostartfiles
endef

define $(package)_preprocess_cmds
endef

define $(package)_config_cmds
  $(if $(filter x86_64%darwin,$(host)),ABI=64 )$($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
