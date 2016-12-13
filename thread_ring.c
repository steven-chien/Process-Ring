#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

/* global barrier and debug flag */
pthread_barrier_t barrier;
int debug;

/* structure for placeholders between threads */
typedef struct synch_t {
	int token;
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
	int *prev_token = &(((proc_arg_t*)arg)->right->token);
	int *next_token = &(((proc_arg_t*)arg)->left->token);

	/* extract mutex lock of placeholder from left and right */
	pthread_mutex_t *prev_mtxp = &(((proc_arg_t*)arg)->right->mtx);
	pthread_mutex_t *next_mtxp = &(((proc_arg_t*)arg)->left->mtx);

	if(debug)
		fprintf(stderr, "thread %d started\n", id);

	/* wait for all other threads before begin */
	pthread_barrier_wait(&barrier);

	/* run for pre defined rounds */
	while(rounds>0) {
		/* acquire mutex of token */
		pthread_mutex_lock(prev_mtxp);

		/* check if token is there */
		if(debug)
			fprintf(stderr, "I'm %d in round %d, token is %d, I received %d, ", id, rounds, *next_token, *prev_token);
	
		*next_token = *prev_token + 1;	// work with token before passing on

		/* remove token and unlock previous */
		*prev_token = -1;
		if(debug)
			fprintf(stderr, "Exchange performed, now token is %d, prev token is %d, unlock next thread!\n", *next_token, *prev_token);
		
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
		placeholders[i]->token = -1;

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
	placeholders[0]->token = 0;	// init the token from the first placeholder
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
		fprintf(stderr, "circulate tokens for %d rounds between %d threads\n", rounds, thread_count);

	double time = benc(rounds, thread_count);
	printf("Time elasped: %lf seconds\n", time);

	return 0;
}
