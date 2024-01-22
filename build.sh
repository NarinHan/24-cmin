#!/bin/bash

set -x

CC="afl-gcc" CFLAGS="-O0 -g -fsanitize=address -static" LDFLAGS="-fsanitize=address" make cminafl
