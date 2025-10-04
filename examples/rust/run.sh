#!/bin/bash
set -e
exec ../../.build/kvmserver -e -t 1 --allow-all storage --1-to-1 target/release/remote ++ run target/release/local
