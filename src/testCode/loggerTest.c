/*
 ============================================================================
 Name        : loggerTest.c
 Author      : Barak Sason Rofman
 Version     :
 Copyright   : Your copyright notice
 Description :
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "logger.h"

#define ITERATIONS 100000
#define NUM_THRDS 10
#define BUF_SIZE 101

#define BUFFSIZE 1000000
#define SHAREDBUFFSIZE 10000000

char chars[] = "0123456789abcdefghijklmnopqrstuvwqxy";
char** data;

void createRandomData(char** data, int charsLen);
void* threadMethod();

int main(void) {
	pthread_t threads[NUM_THRDS];
	int i;
	int charsLen;
	//TODO: remove, for debug only
	struct timeval tv1, tv2;

	if (access("logFile.txt", F_OK) != -1) {
		remove("logFile.txt");
	}

	//TODO: remove, for debug only
	gettimeofday(&tv1, NULL);

	charsLen = strlen(chars);

	initLogger(NUM_THRDS, BUFFSIZE, SHAREDBUFFSIZE);

	data = malloc(NUM_THRDS * sizeof(char*));
	createRandomData(data, charsLen);

	for (i = 0; i < NUM_THRDS; ++i) {
		pthread_create(&threads[i], NULL, threadMethod, data[i]);
	}

	for (i = 0; i < NUM_THRDS; ++i) {
		pthread_join(threads[i], NULL);
	}

	terminateLogger();

	//TODO: remove, for debug only
	printf("Direct writes = %llu\n", cnt);
	gettimeofday(&tv2, NULL);
	printf("Total time = %f seconds\n",
			(double) (tv2.tv_usec - tv1.tv_usec) / 1000000
					+ (double) (tv2.tv_sec - tv1.tv_sec));

	return EXIT_SUCCESS;
}

void createRandomData(char** data, int charsLen) {
	int i;
	for (i = 0; i < NUM_THRDS; ++i) {
		int j;
		data[i] = malloc(BUF_SIZE);
		for (j = 0; j < BUF_SIZE - 2; ++j) {
			data[i][j] = chars[rand() % charsLen];
		}
		data[i][BUF_SIZE - 2] = '\n';
		data[i][BUF_SIZE - 1] = '\0';
	}
}

void* threadMethod(void* data) {
	char* logData = data;

	registerThread(getpid());
	for (int i = 0; i < ITERATIONS; ++i) {
		logMessage(logData);
	}

	return NULL;
}

