#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

pthread_barrier_t barrier;
pthread_barrier_t brp;

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

void *ring_process(void *arg)
{
	int rounds = ((proc_arg_t*)arg)->r;
	int id = ((proc_arg_t*)arg)->id;

	int *prev_token = &(((proc_arg_t*)arg)->right->token);
	int *next_token = &(((proc_arg_t*)arg)->left->token);

	pthread_mutex_t *prev_mtxp = &(((proc_arg_t*)arg)->right->mtx);
	pthread_mutex_t *next_mtxp = &(((proc_arg_t*)arg)->left->mtx);

	printf("thread %d started\n", id);
	pthread_barrier_wait(&barrier);

	while(rounds>0) {
		/* acquire mutex of token */
		pthread_mutex_lock(prev_mtxp);

		/* check if token is there */
		printf("I'm %d, token is %d, I received %d, ", id, *next_token, *prev_token);
		*next_token = *prev_token;

		/* remove token and unlock previous */
		*prev_token = -1;
		printf("Exchange performed, now token is %d, prev token is %d, unlock next thread!\n", *next_token, *prev_token);
		
		/* unlock and signal on conditional */
		pthread_mutex_unlock(next_mtxp);

		rounds--;
	}

	pthread_barrier_wait(&brp);
	
	return NULL;
}

int benc(int p, int r)
{
	pthread_t **threads;
	synch_t **placeholders;
	proc_arg_t **arg;
	struct timespec start_time, end_time;

	/* setup barrier */
	pthread_barrier_init(&barrier, NULL, p+1);
	pthread_barrier_init(&brp, NULL, p+1);

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
	placeholders[0]->token = 999;
	pthread_mutex_unlock(&(placeholders[0]->mtx));

	/* create all thread */
	for(int i=0; i<p; i++) {
		pthread_create(threads[i], NULL, ring_process, (void*)arg[i]);
	}

	/* take time */
	pthread_barrier_wait(&barrier);
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	/* wait until all sign in */
	pthread_barrier_wait(&brp);

	/* start timer */

	/* wait until all done */
	for(int i=0; i<p; i++) {
		if(pthread_join(*threads[i], NULL)) {
			printf("cannot join thread %d\n", i);
			exit(1);
		}
	}

	/* stop timer */
	pthread_barrier_destroy(&barrier);
	pthread_barrier_destroy(&brp);

	/* return result */
}

int main(int argc, char *argv[])
{
	benc(5, 5);

	return 0;
}
