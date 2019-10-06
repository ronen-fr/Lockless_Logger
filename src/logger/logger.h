/*
 ============================================================================
 Name        : Logger_C.c
 Author      : Barak Sason Rofman
 Version     :
 Copyright   : Your copyright notice
 Description :
 ============================================================================
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

//TODO: remove, for debug only
long long cnt;

typedef struct bufferData {
	atomic_uint lastRead;
	atomic_uint lastWrite;
	int bufSize;
	char *buf;
	pthread_t tid;
} bufferData;

void initBufferData(bufferData *bd);
int initLogger(const unsigned int threadsNum, unsigned int privateBuffSize,
               unsigned int sharedBuffSize);
int registerThread();
int logMessage(const char *msg);
void terminateLogger();

#endif /* LOGGER */
