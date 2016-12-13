#!/bin/bash

rm -f *.out
gcc -o bin/thread_ring.out -std=gnu11 -g -Wall -lpthread src/thread_ring.c && ./bin/thread_ring.out -d
gcc -o bin/process_ring.out -std=gnu11 -g -Wall -lpthread src/process_ring.c && ./bin/process_ring.out -d
