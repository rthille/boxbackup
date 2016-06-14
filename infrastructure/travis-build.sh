#!/bin/sh

set -e
set -x

ccache -s

if [ "$BUILD" = 'cmake' ]; then
	cd infrastructure/cmake/build
	cmake --version
	cmake ..
	ctest -V
else
	./bootstrap
	./configure CC="ccache $CC" CXX="ccache $CXX"
	grep CXX config.status
	make
	./runtest.pl ALL $TEST_TARGET
fi

ccache -s
