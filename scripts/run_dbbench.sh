#!/bin/bash
#set -x

NUMTHREAD=1
#BENCHMARKS="customedworkloadzip099write,customedworkloadzip080write,\
#customedworkloaduniformwrite,customedworkloadzip099_4kwrite,\
#customedworkloadzip080_4kwrite,customedworkloaduniform_4kwrite,\
#customedworkloadzip099writemid,customedworkloadzip080writemid,\
#customedworkloaduniformwritemid,customedworkloadzip099_4kwritemid,\
#customedworkloadzip080_4kwritemid,customedworkloaduniform_4kwritemid,\
#customedworkloadzip099writelarge,\
#customedworkloadzip080writelarge,\
#customedworkloaduniformwritelarge,\
#customedworkloadzip099_4kwritelarge,\
#customedworkloadzip080_4kwritelarge,\
#customedworkloaduniform_4kwritelarge"

#BENCHMARKS="customedworkloadzip099write"
#BENCHMARKS="customedworkloadzip080write"
#BENCHMARKS="customedworkloaduniformwrite"
#BENCHMARKS="customedworkloadzip099_4kwrite"
#BENCHMARKS="customedworkloadzip080_4kwrite"
#BENCHMARKS="customedworkloaduniform_4kwrite"
#BENCHMARKS="customedworkloadzip099writemid"
#BENCHMARKS="customedworkloadzip080writemid"
#BENCHMARKS="customedworkloaduniformwritemid"
#BENCHMARKS="customedworkloadzip099_4kwritemid"
#BENCHMARKS="customedworkloadzip080_4kwritemid"
BENCHMARKS="customedworkloaduniform_4kwritemid"

#BENCHMARKS="customedworkloadzip099writelarge,\
#customedworkloadzip080writelarge,\
#customedworkloaduniformwritelarge,\
#customedworkloadzip099_4kwritelarge,\
#customedworkloadzip080_4kwritelarge,\
#customedworkloaduniform_4kwritelarge"

#NoveLSM specific parameters
#NoveLSM uses memtable levels, always set to num_levels 2
#write_buffer_size DRAM memtable size in MBs
#write_buffer_size_2 specifies NVM memtable size; set it in few GBs for perfomance;
OTHERPARAMS="--write_buffer_size=$DRAMBUFFSZ  --nvm_buffer_size=$NVMBUFFSZ"

$DBBENCH/db_bench --threads=$NUMTHREAD --benchmarks=$BENCHMARKS $OTHERPARAMS

#Run all benchmarks
#$APP_PREFIX $DBBENCH/db_bench --threads=$NUMTHREAD --num=$NUMKEYS --value_size=$VALUSESZ \
#$OTHERPARAMS --num_read_threads=$NUMREADTHREADS

