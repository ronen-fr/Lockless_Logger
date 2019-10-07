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
	atomic_int lastRead;
	atomic_int lastWrite;
	int bufSize;
	char* buf;
	pid_t tid;
} bufferData;

int initLogger(const int threadsNum, int privateBuffSize, int sharedBuffSize);
int registerThread(pid_t tid);
int logMessage(const char* msg);
void terminateLogger();

#endif /* LOGGER */
