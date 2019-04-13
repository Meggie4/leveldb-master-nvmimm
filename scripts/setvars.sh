#!/bin/bash
export OLDNVMIMMUSRC=$PWD
export DBBENCH=$OLDNVMIMMUSRC/build
export TEST_TMPDIR=/mnt/ssd
export TEST_MEMDIR=/mnt/pmemdir
#DRAM buffer size in MB
export DRAMBUFFSZ=32
#NVM buffer size in MB
export NVMBUFFSZ=576
