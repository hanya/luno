# Install

## Contents

* [Requirements](#requirements)
* [Build with LuaRocks](#build-with-luarocks)
* [Install with LuaRocks](#install-with-luarocks)
* [Build Loader](#build-loader-and-script-provider)
* [Build Script Provider](#build-script-provider)
* [Install Loader and Script Provider](#install-loader-and-script-provider)

## Requirements

The following things are required to build.

* OpenOffice and its SDK, version 3.4 or newer
* Lua and its header files, version 5.1, 5.2 or Luajit 2.0

## Build with LuaRocks

Luno module depends on the office installation, so a few variables 
should be specified even build with luarocks. This way builds only 
standalone luno module.

Current rockspec file supports only Linux and Windows environments. 
It can be improved for other environment.

Here is an example to build the module against Apache OpenOffice 3.4 
for LuaJIT on Linux environment.

    luarocks build luno-version.rockspec \
        LUA_DIR=/usr/local \
        LUA_INC_DIR=/usr/local/include/luajit-2.0 \
        LUA_LIB_SUFFIX=jit-5.1 \
        SDK_DIR=/opt/openoffice.org/basis3.4/sdk \
        URE_DIR=/opt/openoffice.org/ure \

  - LUA_DIR: used to find Lua library to link
  - LUA_INC_DIR: used to find Lua headers
  - LUA_LIB_SUFFIX: connected to name to the lua library to link
  - SDK_DIR: path to SDK of the office, to find include files and link libraries
  - URE_DIR: path to URE of the office, to find link libraries

## Install with LuaRocks

If any compiler is installed on the environment, get binary rock and 
install it with install command.

    luarocks build luno-version-environment.rock

## Build Loader

This section does not a part of LuaRocks.

    > source ~/openoffice.org3.4_sdk/host_name/setsdkenv_unix.sh
    > rake loader LUA_DIR=path_to_lua_dir

Build Lua or LuaJIT as follows and specify as LUA_DIR:

    > make
    > make install PREFIX=path_to_build_dir/installed

HOST should be specified with non mswin Rake on Windows: 

    > rake loader LUA_DIR=path_to_lua_dir HOST=mswin

## Build Script Provider

Packing only.

    > rake provider

## Install Loader and Script Provider

Install the package for LuaLoader first through the extension manager. 
Then restart the office and then install the ProviderForLua package next.
