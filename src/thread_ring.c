/*
 * Copyright 2016 W.D. Chien
 *
 * This file is part of process ring.
 * process ring is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Ring is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MSG_SIZE 50

/* global barrier and debug flag */
pthread_barrier_t barrier;
int debug;

/* structure for placeholders between threads */
typedef struct synch_t {
	char msg[MSG_SIZE];
	pthread_mutex_t mtx;
} synch_t;

/* structure for arguments to thread callback */
typedef struct proc_arg_t {
	int id;
	int p;
	int r;

	/* pointer to the left and right placeholder */
	synch_t *left;
	synch_t *right;
} proc_arg_t;

/* thread callback */
void *ring_process(void *arg)
{
	/* extract id info */
	int rounds = ((proc_arg_t*)arg)->r;
	int id = ((proc_arg_t*)arg)->id;

	/* extract pointer to token placeholder in left and right */
	char *prev_msg = (((proc_arg_t*)arg)->right->msg);
	char *next_msg = (((proc_arg_t*)arg)->left->msg);

	/* extract mutex lock of placeholder from left and right */
	pthread_mutex_t *prev_mtxp = &(((proc_arg_t*)arg)->right->mtx);
	pthread_mutex_t *next_mtxp = &(((proc_arg_t*)arg)->left->mtx);

	if(debug)
		fprintf(stderr, "Thread %d started!\n", id);

	/* wait for all other threads before begin */
	pthread_barrier_wait(&barrier);

	/* run for pre defined rounds */
	while(rounds>0) {
		/* acquire mutex of token */
		pthread_mutex_lock(prev_mtxp);

		/* extracting sender and token from sender */
		int sender, token;
		char *tokenizer_state;

		char *tokenizer = strtok_r(prev_msg, ";", &tokenizer_state);
		sender = atoi(tokenizer);
		tokenizer = strtok_r(NULL, ";", &tokenizer_state);
		token = atoi(tokenizer);

		/* check if token is there */
		if(debug)
			fprintf(stderr, "I'm %d in round %d, I received %d from %d, ", id, rounds, token, sender);

		/* remove token and unlock previous */
		token++;
		snprintf(prev_msg, MSG_SIZE, "-1;-1");
		snprintf(next_msg, MSG_SIZE, "%d;%d", id, token);

		if(debug)
			fprintf(stderr, "pass on message %s and unlock next thread!\n", next_msg);
		
		/* unlock mutex of next placeholder to signal the next thread */
		pthread_mutex_unlock(next_mtxp);

		rounds--;
	}

	return NULL;
}

double benc(int p, int r)
{
	pthread_t **threads;
	synch_t **placeholders;
	proc_arg_t **arg;
	struct timespec start_time, end_time;

	/* setup barrier */
	pthread_barrier_init(&barrier, NULL, p+1);

	/* allocate space for pthread arguments */
	threads = (pthread_t**)malloc(sizeof(pthread_t*)*p);
	placeholders = (synch_t**)malloc(sizeof(synch_t*)*p);
	arg = (proc_arg_t**)malloc(sizeof(proc_arg_t*)*p);

	for(int i=0; i<p; i++) {
		threads[i] = (pthread_t*)malloc(sizeof(pthread_t));
		arg[i] = (proc_arg_t*)malloc(sizeof(proc_arg_t));

		placeholders[i] = (synch_t*)malloc(sizeof(synch_t));
		snprintf(placeholders[i]->msg, MSG_SIZE, "-1;-1");;

		pthread_mutex_init(&(placeholders[i]->mtx), NULL);
		pthread_mutex_lock(&(placeholders[i]->mtx));	// lock all mutex so all processes will be waiting except the one that's active
	}

	/* init thread arguments */
	for(int i=0; i<p; i++) {
		arg[i]->r = r;
		arg[i]->p = p;
		arg[i]->id = i;

		/* setup pointers to placeholders for each thread */
		if(i==p-1) {
			arg[i]->left = placeholders[0];
		}
		else {
			arg[i]->left = placeholders[i+1];
		}

		arg[i]->right = placeholders[i];
	}

	/* take lock of one placeholder */
	snprintf(placeholders[0]->msg, MSG_SIZE, "-1;0");	// init the token from the first placeholder
	pthread_mutex_unlock(&(placeholders[0]->mtx));	// unlock the first mutex so that it starts from the first thread

	/* create all thread */
	for(int i=0; i<p; i++) {
		pthread_create(threads[i], NULL, ring_process, (void*)arg[i]);
	}

	/* take time */

	/* wait until all sign in */
	pthread_barrier_wait(&barrier);

	/* start timer */
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	/* wait until all done */
	for(int i=0; i<p; i++) {
		if(pthread_join(*threads[i], NULL)) {
			fprintf(stderr, "cannot join thread %d\n", i);
			exit(1);
		}
	}

	/* stop timer */
	clock_gettime(CLOCK_MONOTONIC, &end_time);

	/* cleanup */
	pthread_barrier_destroy(&barrier);

	for(int i=0; i<p; i++) {
		pthread_mutex_destroy(&(placeholders[i]->mtx));
		free(placeholders[i]);
		free(arg[i]);
		free(threads[i]);
	}

	free(placeholders);
	free(arg);
	free(threads);

	/* return result */
	return ((double)end_time.tv_sec + 1.0e-9 * end_time.tv_nsec) - ((double)start_time.tv_sec + 1.0e-9 * start_time.tv_nsec);
}

int main(int argc, char *argv[])
{
	int rounds=5, thread_count=5, c;
	debug = 0;

	/* extract cmd arguments */
	while((c=getopt(argc, argv, "dp:r:h")) != -1) {
		switch (c) {
			case 'h':
				fprintf(stderr, "Usage: ./ring_thread -p [thread count] -r [rounds]\n");
				return 0;
			case 'd':
				debug = 1;
				break;
			case 'p':
				thread_count = atoi(optarg);
				break;
			case 'r':
				rounds = atoi(optarg);
				break;
			case '?':
				if(optopt=='p' || optopt=='r')
					fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				else if (isprint(optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				return -1;
			default:
				thread_count = 5;
				rounds = 5;
		}
	}

	if(debug)
		fprintf(stderr, "Circulate token for %d rounds between %d threads\n", rounds, thread_count);

	double time = benc(rounds, thread_count);
	printf("Time elasped: %lf seconds\n", time);

	return 0;
}
