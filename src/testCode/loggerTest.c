/*
 * loggerTest.c
 *
 *  Created on: Sep 15, 2019
 *      Author: bsasonro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "logger.h"

#define ITERATIONS 10000
#define NUM_THRDS 100
#define BUF_SIZE 128

char chars[] = "0123456789abcdefghijklmnopqrstuvwqxy";
char **data;

void createRandomData(char **data, int charsLen);
void* threadMethod();

int main(void) {
	pthread_t threads[NUM_THRDS];
	int i;
	int charsLen = strlen(chars);

	initLogger(NUM_THRDS);

	data = malloc(NUM_THRDS * sizeof(char*));
	createRandomData(data, charsLen);

	for (i = 0; i < NUM_THRDS; ++i) {
		pthread_create(&threads[i], NULL, threadMethod, data[i]);
	}

	for (i = 0; i < NUM_THRDS; ++i) {
		pthread_join(threads[i], NULL);
	}

	sleep(1);

	terminateLogger();

	//TODO: Remove (for debugging)
	printf("Waiting:\nseq = %d, wrap = %d\n", seq, wrap);

	return EXIT_SUCCESS;
}

void createRandomData(char **data, int charsLen) {
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

void* threadMethod(void *data) {
	char *logData;

	logData = data;

	registerThread();
	for (int i = 0; i < ITERATIONS; ++i) {
		logMessage(logData);
	}
	return NULL;
}

