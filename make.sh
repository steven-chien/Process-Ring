#!/bin/bash

gcc -o ring_thread.out -std=gnu11 -g -Wall -lpthread ring_thread.c
gcc -o ring_process.out -std=gnu11 -g -Wall -lpthread ring_process.c
