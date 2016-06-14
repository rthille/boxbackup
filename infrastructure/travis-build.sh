#!/bin/sh

set -e
set -x

ccache -s

if [ "$BUILD" = 'cmake' ]; then
	export CC="ccache $CC"
	export CXX="ccache $CXX"
	cd infrastructure/cmake/build
	cmake --version
	cmake -DCMAKE_BUILD_TYPE:STRING=Debug ..
	ctest -V
else
	./bootstrap
	./configure CC="ccache $CC" CXX="ccache $CXX"
	grep CXX config.status
	make
	./runtest.pl ALL $TEST_TARGET
fi

ccache -s
