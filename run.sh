#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
BUILD_DIR="$SCRIPT_DIR/.build"
set -e
git submodule update --init
mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR"
cmake $SCRIPT_DIR -DCMAKE_BUILD_TYPE=Release
make -j 8
popd

# When PERF=1 run with perf
if [ "$PERF" = "1" ]; then
	perf record -g --call-graph dwarf -- $BUILD_DIR/kvmserver "$@"
	perf script | c++filt > perf.out
	echo "perf.out generated"
	exit 0
elif [ "$GDB" = "1" ]; then
	# When GDB=1 run with gdb
	gdb --args $BUILD_DIR/kvmserver "$@"
	exit 0
else
	$BUILD_DIR/kvmserver "$@"
fi
