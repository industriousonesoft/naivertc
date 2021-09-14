#!/bin/bash

cd "$(dirname) $0"

# -e(-o errexit): Exit when encountering error
# -x(-o xtrace): Print the command and involved parameters after executed
# -u(-o nounset): Print error and exit when encountering a undefined variable 
set -ex

RELATIVE_PATH="$(PWD)"
SOURCE_DIR="$RELATIVE_PATH/_source"
CACHE_DIR="$RELATIVE_PATH/_cache"
BUILD_DIR="$RELATIVE_PATH/_build"
INSTALL_DIR="$RELATIVE_PATH/_install"

mkdir -p $SOURCE_DIR
mkdir -p $CACHE_DIR
mkdir -p $BUILD_DIR
mkdir -p $INSTALL_DIR

source "$RELATIVE_PATH/VERSION"

# fetch the logical CPU numbers
JOBS=""
if [ -z "$JOBS" ]; then 
    JOBS="$(sysctl -n hw.logicalcpu_max)"
fi
echo "JOBS= $JOBS"

# build Boost library
BOOST_VERSION_FILE="$INSTALL_DIR/boost.version"
BOOST_VERSION_CHANGED=0
if [ ! -e $BOOST_VERSION_FILE -o "$BOOST_BUILD_VERSION" != "$(cat $BOOST_VERSION_FILE)" ]; then
    BOOST_VERSION_CHANGED=1
fi

if [ $BOOST_VERSION_CHANGED -eq 1 -o ! -e $INSTALL_DIR/boost/lib/libboost_filesystem.a ]; then
    # fetch Boost source codes
    rm -rf $SOURCE_DIR/boost
    ./fetch_boost.sh $BOOST_BUILD_VERSION $SOURCE_DIR/boost $CACHE_DIR/boost

    # build 
    rm -rf $BUILD_DIR/boost
    rm -rf $INSTALL_DIR/boost
    pushd $SOURCE_DIR/boost/source
        # 此处使用clang++作为boost的编译器，且配合专门为clang定值的c++标准库：libcxx
        # clang++和libcxx库可以从llvm项目下载，此处使用xcode中自带的这两个库
        # 在project-config.jam中指定使用clang++库，而非gcc(c++标准库为std++)库，cxxflags中通过-isystem指定libcxx的头文件路径，-nostdinc++表示不使用stdc++(虽然clang也支持，但是此处使用libcxx)
        echo "using clang : : /usr/bin/clang++ : ;" > project-config.jam
        SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
        #使用b2命令编译boost库
        ./b2 \
            cflags=" \
                --sysroot=$SYSROOT \
            " \
            cxxflags=" \
                -isystem /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1 \
                -nostdinc++ \
                --sysroot=$SYSROOT \
            " \
            toolset=clang \
            visibility=hidden \
            link=static \
            variant=release \
            install \
            -j$JOBS \
            --build-dir=$BUILD_DIR/boost \
            --prefix=$INSTALL_DIR/boost \
            --ignore-site-config \
            --with-filesystem \
            --with-thread
    popd

fi
echo $BOOST_BUILD_VERSION > $BOOST_VERSION_FILE