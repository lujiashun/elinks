export LIBRARY_PATH="$HOME/lib"
export PKG_CONFIG_PATH="$HOME/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/include"
export CFLAGS="-O2 -I$HOME/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"
export CXXFLAGS="-O2 -I$HOME/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"
export LDFLAGS="-L$HOME/lib"
export CC="i586-pc-msdosdjgpp-gcc"
export AR="i586-pc-msdosdjgpp-ar"
cd
wget http://download.netsurf-browser.org/libs/releases/libdom-0.4.1-src.tar.gz
rm -rf libdom-0.4.1
tar -xf libdom-0.4.1-src.tar.gz
make -C libdom-0.4.1 install -j1 Q= PREFIX=$HOME LIBDIR=lib COMPONENT_TYPE=lib-static