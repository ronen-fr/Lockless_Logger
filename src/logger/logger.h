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
#include "statusCodes.h"

#define BUFFSIZE 1048576

//TODO: Remove (for debugging)
int seq;
int wrap;

typedef struct bufferData {
	atomic_bool isWaiting;
	atomic_int lastRead;
	atomic_int lastWrite;
	int bufSize;
	char* buf;
	pthread_t tid;
	sem_t sem;
} bufferData;

void initBufferData(bufferData* bd);
bool initLogger(int threadsNum);
void registerThread();
bool logMessage(char* msg);
void terminateLogger();

#endif /* LOGGER */
