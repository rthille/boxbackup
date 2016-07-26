#!/bin/sh

set -e
set -x

ccache -s

if [ "$BUILD" = 'cmake' ]; then
	cd `dirname $0`
	mkdir -p cmake/build
	cd cmake/build
	cmake --version
	cmake -DCMAKE_BUILD_TYPE:STRING=Debug ..
	make install
	[ "$TEST" = "n" ] || ctest -V
else
	cd `dirname $0`/..
	./bootstrap
	./configure CC="ccache $CC" CXX="ccache $CXX"
	grep CXX config.status
	make
	./runtest.pl ALL $TEST_TARGET
fi

ccache -s
