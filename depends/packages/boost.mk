package=boost
$(package)_version=1.81.0
$(package)_download_path=https://archives.boost.io/release/$($(package)_version)/source/
$(package)_file_name=boost_$(subst .,_,$($(package)_version)).tar.gz
$(package)_sha256_hash=205666dea9f6a7cfed87c7a6dfbeb52a2c1b9de55712c9c1a87735d7181452b6

define $(package)_set_vars
$(package)_config_opts_release=variant=release
$(package)_config_opts_debug=variant=debug
$(package)_config_opts=--layout=tagged --build-type=complete --user-config=user-config.jam
$(package)_config_opts+=threading=multi link=static -sNO_BZIP2=1 -sNO_ZLIB=1
$(package)_config_opts_linux=threadapi=pthread runtime-link=shared
$(package)_config_opts_darwin=--toolset=clang-darwin runtime-link=shared
$(package)_config_opts_mingw32=binary-format=pe target-os=windows threadapi=win32 runtime-link=static
$(package)_config_opts_x86_64_mingw32=address-model=64
$(package)_config_opts_i686_mingw32=address-model=32
$(package)_config_opts_i686_linux=address-model=32 architecture=x86
$(package)_toolset_$(host_os)=gcc
$(package)_archiver_$(host_os)=$($(package)_ar)
$(package)_toolset_darwin=clang-darwin
$(package)_config_libraries=chrono,filesystem,program_options,system,thread,test
$(package)_cppflags_darwin=
$(package)_cxxflags_darwin=-std=c++1z -nostdinc++ -Xclang -fno-cxx-modules -iwithsysroot/usr/include/c++/v1 -iwithsysroot/usr/include
$(package)_cxxflags_mingw32=-std=c++17
endef

define $(package)_preprocess_cmds
  echo "using $(boost_toolset_$(host_os)) : : $($(package)_cxx) : <cxxflags>\"$($(package)_cxxflags) $(if $(filter darwin,$(host_os)),,$($(package)_cppflags))\" <linkflags>\"$($(package)_ldflags)\" <archiver>\"$(boost_archiver_$(host_os))\" <striper>\"$(host_STRIP)\"  <ranlib>\"$(host_RANLIB)\" <rc>\"$(host_WINDRES)\" : ;" > user-config.jam
endef

define $(package)_config_cmds
  ./bootstrap.sh --without-icu --with-libraries=$(boost_config_libraries)
endef

define $(package)_build_cmds
  ./b2 -d2 -j2 -d1 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) stage
endef

define $(package)_stage_cmds
  ./b2 -d0 -j4 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) install
endef