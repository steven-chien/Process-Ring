#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MSG_SIZE 50

int debug;

void ring_process(int id, int rounds, int *left, int *right, int *report_channel)
{
	/* close read end on left hand side and make sure parent is alive */
	int pid = getpid();
	int ready_flag = 1;
	char msg[MSG_SIZE];
	close(left[1]);
	
	if(debug)
		fprintf(stderr, "Process %d(%d) started!\n", id, pid);

	/* alert parent that process is initialized and ready */
	close(report_channel[0]);
	write(report_channel[1], &ready_flag, sizeof(int));

	/* check if parent is still alive and watch the read end of left hand side pipe for message from neighbor */
	while(getppid()!=1 && (read(left[0], msg, sizeof(msg)))>0 && rounds>0) {
		int token, sender;

		/* extract sender PID and token from message */
		char *tokenizer = strtok(msg, ";");
		sender = atoi(tokenizer);
		tokenizer = strtok(NULL, ";");
		token = atoi(tokenizer);

		if(debug)
			fprintf(stderr, "I'm %d(%d) in round %d, received token %d from %d, increment and send \"", id, getpid(), rounds, token, sender);

		/* play with token and prepare message */
		token++;
		snprintf(msg, MSG_SIZE, "%d;%d", pid, token);

		if(debug)
			printf("%s\" to right hand side\n", msg);

		/* pass message to right hand side */
		close(right[0]);
		write(right[1], msg, sizeof(msg));

		/* decrement round and reset token value */
		rounds--;
		token = -1;
	}
}

double master(int *pipe)
{
	/* prepare benchmark and message */
	struct timespec start_time, end_time;
	char msg[MSG_SIZE];
	snprintf(msg, MSG_SIZE, "%d;%d", getpid(), 1);

	if(debug)
		fprintf(stderr, "Parent(%d) kick starting by inserting token message %s...\n", getpid(), msg);

	/* take time */
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	/* write to first child to kick start process */
	close(pipe[0]);
	write(pipe[1], msg, sizeof(msg));

	/* wait for all children */
	wait(NULL);

	/* take end time */
	clock_gettime(CLOCK_MONOTONIC, &end_time);

	/* return time elasped */
	return ((double)end_time.tv_sec + 1.0e-9 * end_time.tv_nsec) - ((double)start_time.tv_sec + 1.0e-9 * start_time.tv_nsec);
}

int main(int argc, char *argv[])
{
	/* default round and process */
	int rounds=5, process_count=5, c;
	debug = 0;

	while((c=getopt(argc, argv, "dp:r:h")) != -1) {
		switch (c) {
			case 'h':
				fprintf(stderr, "Usage: ./process_ring.out -p [process count] -r [rounds], optionally -d for debug message.\n");
				return 0;
			case 'd':
				debug = 1;
				break;
			case 'p':
				process_count = atoi(optarg);
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
				process_count = 5;
				rounds = 5;
		}
	}

	if(debug)
		fprintf(stderr, "Circulate token for %d rounds between %d processes\n", rounds, process_count);
	
	/* create pipes (to connect processes) and fork pid placeholder */
	int **pipes, **report_channel;
	int pid;

	pipes = (int**)malloc(sizeof(int*)*process_count);
	report_channel = (int**)malloc(sizeof(int*)*process_count);

	for(int i=0; i<process_count; i++) {
		pipes[i] = (int*)malloc(sizeof(int)*2);
		report_channel[i] = (int*)malloc(sizeof(int)*2);

		pipe(pipes[i]);
		pipe(report_channel[i]);
	}

	/* fork children and kick start ring process */
	for(int i=0; i<process_count; i++) {
		pid = fork();
		
		if(pid==0) {
			if(i==process_count-1)
				ring_process(i, rounds, pipes[0], pipes[i], report_channel[i]);
			else
				ring_process(i, rounds, pipes[i+1], pipes[i], report_channel[i]);
			break;
		}
	}

	if(pid!=0) {
		/* wait for all children to be ready (like pthread_barrier) */
		int ready_count = 0;
		while(ready_count<process_count) {
			int msg = 0;
			read(report_channel[ready_count][0], &msg, sizeof(int));

			if(msg==1) {
				ready_count++;
			}
			else {
				fprintf(stderr, "Received error from child, exiting...\n");
				return 1;
			}
		}

		/* writing token message to to first child to kick start process */
		double elasped = master(pipes[0]);
		printf("Time elasped: %lf seconds\n", elasped);
	}

	/* cleanup */
	for(int i=0; i<process_count; i++) {
		free(pipes[i]);
		free(report_channel[i]);
	}
	free(pipes);
	free(report_channel);

	return 0;
}
