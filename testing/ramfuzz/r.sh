#!/bin/bash
S=~/src/chromium/src
~/bin/llvm/rel/bin/ramfuzz \
    $S/net/disk_cache/memory/mem_backend_impl.h \
    $S/base/callback.h \
    $S/base/time/time.h \
    -- -I$S -x c++ -std=c++14
