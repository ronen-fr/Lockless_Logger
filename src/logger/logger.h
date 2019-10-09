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

#include <pthread.h>
#include <stdatomic.h>
#include <sys/time.h>

//TODO: remove, for debug only
long long cnt;

typedef struct bufferData {
	atomic_int lastRead;
	atomic_int lastWrite;
	int bufSize;
	char* buf;
	pthread_t tid;
} bufferData;

typedef struct messageInfo {
	char* file;
	const char* func;
	int line;
	int msgLen;
	int millisec;
	pthread_t tid;
	struct tm* tm_info;
	struct timeval tv;
//TODO: handle log levels
//	int level;
} messageInfo;

int initLogger(const int threadsNum, int privateBuffSize, int sharedBuffSize);
int registerThread(pthread_t tid);

/* 'logMessage' should be called only by using the macro 'LOG_MSG' */
int logMessage(char* file, const char* func, const int line, const char* msg,
               ...);

void terminateLogger();

#define LOG_MSG(msg ...) logMessage(__FILE__, __PRETTY_FUNCTION__, __LINE__, msg)

#endif /* LOGGER */
