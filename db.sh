#!/bin/bash
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo /usr/local/src/ldb/ -DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer && cmake --build  . --target db_bench --clean-first -j 40
mv db_bench db_bs
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo /usr/local/src/ldb/ -DCMAKE_CXX_FLAGS="-DDO_SIP -fno-omit-frame-pointer" && cmake --build . --target db_bench --clean-first -j 40
mv db_bench db_sip
B="fillseq"
for i in $(seq 75); do B="$B,readrandom" ; done
B32="fillseq"
for i in $(seq 20); do B32="$B32,readrandom" ; done
#./db_bs --benchmarks="$B" --db=/mnt/sda4 --use_existing_db=0 --write_buffer_size=$((32*10**6)) --block_size=4096 --num=$((80*10**6))
#./db_sip --benchmarks="$B" --db=/mnt/sda4 --use_existing_db=0 --write_buffer_size=$((32*10**6)) --block_size=131072 --num=$((80*10**6))
./db_bs --benchmarks="$B32" --db=/mnt/sda4 --use_existing_db=0 --write_buffer_size=$((32*10**6)) --block_size=4096 --num=$((4*80*10**6))
./db_sip --benchmarks="$B32" --db=/mnt/sda4 --use_existing_db=0 --write_buffer_size=$((32*10**6)) --block_size=131072 --num=$((4*80*10**6))
