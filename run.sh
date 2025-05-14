#!/bin/bash
set -e
mkdir -p .build
pushd .build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j 8
popd

# When PERF=1 run with perf
if [ "$PERF" = "1" ]; then
	perf record -g --call-graph dwarf -- .build/kvmserver "$@"
	perf script | c++filt > perf.out
	echo "perf.out generated"
	exit 0
else
	./.build/kvmserver "$@"
fi
