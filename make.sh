#!/bin/bash

rm -f *.out
gcc -o thread_ring.out -std=gnu11 -g -Wall -lpthread thread_ring.c && ./thread_ring.out -d
gcc -o process_ring.out -std=gnu11 -g -Wall -lpthread process_ring.c && ./process_ring.out -d
