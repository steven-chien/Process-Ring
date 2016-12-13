#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

pthread_barrier_t barrier;
int debug;

typedef struct synch_t {
	int token;
	pthread_mutex_t mtx;
} synch_t;

typedef struct proc_arg_t {
	int id;
	int p;
	int r;
	synch_t *left;
	synch_t *right;
} proc_arg_t;

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
	if ((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}

	return;
}

void *ring_process(void *arg)
{
	int rounds = ((proc_arg_t*)arg)->r;
	int id = ((proc_arg_t*)arg)->id;

	int *prev_token = &(((proc_arg_t*)arg)->right->token);
	int *next_token = &(((proc_arg_t*)arg)->left->token);

	pthread_mutex_t *prev_mtxp = &(((proc_arg_t*)arg)->right->mtx);
	pthread_mutex_t *next_mtxp = &(((proc_arg_t*)arg)->left->mtx);

	if(debug)
		fprintf(stderr, "thread %d started\n", id);
	pthread_barrier_wait(&barrier);

	while(rounds>0) {
		/* acquire mutex of token */
		pthread_mutex_lock(prev_mtxp);

		/* check if token is there */
		if(debug)
			fprintf(stderr, "I'm %d in round %d, token is %d, I received %d, ", id, rounds, *next_token, *prev_token);
		*next_token = *prev_token + 1;

		/* remove token and unlock previous */
		*prev_token = -1;
		if(debug)
			fprintf(stderr, "Exchange performed, now token is %d, prev token is %d, unlock next thread!\n", *next_token, *prev_token);
		
		/* unlock and signal on conditional */
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
		pthread_mutex_lock(&(placeholders[i]->mtx));
	}

	/* init thread arguments */
	for(int i=0; i<p; i++) {
		arg[i]->r = r;
		arg[i]->p = p;
		arg[i]->id = i;

		if(i==p-1) {
			arg[i]->left = placeholders[0];
		}
		else {
			arg[i]->left = placeholders[i+1];
		}

		arg[i]->right = placeholders[i];
	}

	/* take lock of one placeholder */
	placeholders[0]->token = 0;
	pthread_mutex_unlock(&(placeholders[0]->mtx));

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
	printf("start: %ld.%ld; end: %ld.%ld\n", start_time.tv_sec, start_time.tv_nsec, end_time.tv_sec, end_time.tv_nsec);

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

	while((c=getopt(argc, argv, "dp:r:")) != -1) {
		switch (c) {
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
	printf("seconds elasped: %lf\n", time);

	return 0;
}
