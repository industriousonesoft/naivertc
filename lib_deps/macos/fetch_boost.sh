#!/bin/bash

if [ $# -lt 3 ]; then
    echo "$0 <boost_version> <output_dir> <cache_dir>"
    exit 1
fi

BOOST_VERSION=$1
BOOST_VERSION_UNDERSCORE=${BOOST_VERSION//./_}
OUTPUT_DIR=$2
CACHE_DIR=$3

set -ex

mkdir -p $OUTPUT_DIR

pushd $OUTPUT_DIR
    if [ ! -e $CACHE_DIR/boost_$BOOST_VERSION_UNDERSCORE.tar.gz ]; then
        mkdir -p $CACHE_DIR
        # -L: location
        # -o: output <file>
        curl -Lo $CACHE_DIR/boost_$BOOST_VERSION_UNDERSCORE.tar.gz https://boostorg.jfrog.io/artifactory/main/release/$BOOST_VERSION/source/boost_$BOOST_VERSION_UNDERSCORE.tar.gz
    fi

    rm -rf source
    rm -rf boost_$BOOST_VERSION_UNDERSCORE
    tar xf $CACHE_DIR/boost_$BOOST_VERSION_UNDERSCORE.tar.gz
    mv boost_$BOOST_VERSION_UNDERSCORE source

    pushd source
        ./bootstrap.sh
    popd
popd