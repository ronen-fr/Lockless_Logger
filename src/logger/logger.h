/*
 * logger.h
 *
 *  Created on: Sep 15, 2019
 *      Author: bsasonro
 */

#ifndef LOGGER
#define LOGGER

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>

#define BUFFSIZE 1000000
#define SHAREDBUFFSIZE 1000000

typedef struct bufferData {
	atomic_uint lastRead;
	atomic_uint lastWrite;
	int bufSize;
	char *buf;
	pthread_t tid;
} bufferData;

void initBufferData(bufferData *bd);
int initLogger(const unsigned int threadsNum);
int registerThread();
int logMessage(const char *msg);
void terminateLogger();

#endif /* LOGGER */
