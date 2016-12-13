### Introduction

A hobby implementation of process ring with both thread and process version.

### Usage
Compile:
```bash
./make.sh
```

Thread version: 
```bash
./bin/thread_ring.out -p 5 -r 5 -d
```
Process version:
```bash
./bin/process_ring.out -p 5 -r 5 -d
```

The -p flag specifies how many process/thread in the ring; the -r flag specifies how many rounds will be token be passed around the ring; the -d flag specifies that the program will print operation messages on console. If nothing is specified the program will use the default value 5 for both rounds and thread count.

### Thread version

The prorgram creates the specified number of threads and respective number of placeholders. The placeholders are imaginary channels between neighboring thread in a ring. Each placeholder contains a string message and a mutex. Each thread will wait for the placeholder on the left. Once the lock is acquired the thread will extract and parse the message inside the holder. The thread will manipulate the token in the message and reset the message. The string message is in the format (id);(token) where the id is the order when the thread is created and token is an integer. The thread will prepare a new message and place it in the placeholder on the right. The thread unlocks the mutex to the placeholder on the right and the thread on the right will be able to acquire the lock. Thus the threads can be executed in the (imaginary) clockwise direction. At the begining all the mutex are locked and the parent will unlock the first placeholder so that the first child on the right can acquire the lock and operate on it. The threads are synchornized with pthread barrier.

### Process version

The program creates the specified number of process, inter process pipes and parent to child pipes. All child operations are execuited in a function that takes the left hand side pipe, right hand side pipe and parent pipe. Once ready, the child will send a message to the parent notifying that it's ready through the parenet pipe. All child wait for message in a loop for the message from the left pipe. Once they receive a message it will be parsed and prepared and sent through the right pipe. The parent will wait for all ready signals from children before starting the benchmarking. Once all the ready signals are received the parent injects a message on the left pipe that the first child holds. The process is thus kick started.

### Benchmarking

Both programs synchronizes for the starting time and ending time so to benchmark for the start and end time. The timestamp is taken with clock_getting() in nanoseconds. The difference is reported through the console.

### License

This file is part of Process Ring.

Process Ring is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Foobar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

