/*
 ============================================================================
 Name        : Logger_C.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "logger.h"

static bool isTerminate;
static int logFile;
static int bufferDataArraySize;
static int nextFreeCell;
static bufferData **bufferDataArray;
static pthread_mutex_t lock;
static pthread_t loggerThread;

static bool createDataFile();
static void* runLogger();
static bufferData* getBuffer();

void initBufferData(bufferData *bd) {
	bd->bufSize = BUFFSIZE;
	//TODO: think if malloc failures need to be handled
	bd->buf = malloc(BUFFSIZE);
	sem_init(&bd->sem, 0, 0);
}

bool initLogger(int threadsNum) {
	int i;

	bufferDataArraySize = threadsNum;
	pthread_mutex_init(&lock, NULL);

	if (STATUS_SUCCESS != createDataFile()) {
		//TODO: handle error
		return STATUS_FAILURE;
	}

	//TODO: think if malloc failures need to be handled
	bufferDataArray = malloc(threadsNum * sizeof(bufferData*));
	for (i = 0; i < bufferDataArraySize; ++i) {
		bufferDataArray[i] = malloc(sizeof(bufferData));
		initBufferData(bufferDataArray[i]);
	}

	pthread_create(&loggerThread, NULL, runLogger, NULL);

	return STATUS_SUCCESS;
}

static bool createDataFile() {
	//TODO: handle case of existing file
	//TODO: add header message for current log file
	//TODO: remove O_TRUNC and implement rotating logging
	logFile = open("logFile.txt", O_RDWR | O_CREAT | O_TRUNC, 0777);
	if (-1 == logFile) {
		//TODO: handle error
		return STATUS_FAILURE;
	}
	return STATUS_SUCCESS;
}

static void* runLogger() {
	atomic_bool isWaiting;
	atomic_int lastWrite;
	int lastRead;
	int lenToBufEnd;
	int i;
	int dataLen;
	bufferData *bd;

	while (false == isTerminate) {
		for (i = 0; i < bufferDataArraySize; ++i) {
			bd = bufferDataArray[i];
			if (NULL != bd) {
				__atomic_load(&bd->lastWrite, &lastWrite, __ATOMIC_SEQ_CST);
				lastRead = bd->lastRead;
				dataLen = lastWrite - lastRead;
				/* If lastWrite != lastRead, there is new data to write
				 * Handle 2 cases of new data */
				if (0 < dataLen) {
					/* Sequential write */
					write(logFile, bd->buf + lastRead, dataLen);
				} else if (0 > dataLen) {
					/* Wrap around write */
					lenToBufEnd = bd->bufSize - lastRead;
					write(logFile, bd->buf + lastRead, lenToBufEnd);
					write(logFile, bd->buf, lastWrite);
				}
				/* Flush data to disk */
				fsync(logFile);

				/* Update new lastRead */
				__atomic_store_n(&bd->lastRead, lastWrite,
				__ATOMIC_SEQ_CST);

				/* Check if worker thread is waiting on semaphore and
				 * only release after all data in the buffer has been written to file*/
				__atomic_load(&bd->isWaiting, &isWaiting, __ATOMIC_SEQ_CST);
				if ((true == isWaiting) && (lastRead == lastWrite)) {
					__atomic_store_n(&bd->isWaiting, false, __ATOMIC_SEQ_CST);
					sem_post(&bd->sem);
				}
			}
		}
	}

	return NULL;
}

/* Terminate the logger thread and release resources */
void terminateLogger() {
	isTerminate = true;
	pthread_join(loggerThread, NULL);
}

void registerThread() {
	pthread_mutex_lock(&lock);

	//TODO: return an error if trying to add more thread buffers than allowed
	if (bufferDataArraySize == nextFreeCell) {
//		return NULL;
	}

	bufferDataArray[nextFreeCell++]->tid = pthread_self();

	pthread_mutex_unlock(&lock);
}

static bufferData* getBuffer() {
	int i;
	bufferData *bd;
	pthread_t thrd = pthread_self();

	for (i = 0; i < nextFreeCell; ++i) {
		bd = bufferDataArray[i];
		if (bd->tid == thrd) {
			return bd;
		}
	}

	printf("how???\n");
	return NULL;
}

bool logMessage(char *msg) {
	int msgLen;
	int lenToBufEnd;
	int lastRead;
	atomic_int lastWrite;
	bufferData *bd;

	//TODO: implement rotating file write
	bd = getBuffer();
	if (NULL == bd) {
		return STATUS_FAILURE;
	}

	msgLen = strlen(msg);
	__atomic_load(&bd->lastRead, &lastRead, __ATOMIC_SEQ_CST);
	lastWrite = bd->lastWrite;
	lenToBufEnd = bd->bufSize - lastWrite;

	/* Handle 2 cases of potential data override */
	if (lastWrite <= lastRead && ((lastWrite + msgLen) >= lastRead)) {
		/* Next write will over shot past lastRead */
		/* Shouldn't happen - increase buffer size */
		__atomic_store_n(&bd->isWaiting, true, __ATOMIC_SEQ_CST);
		++seq; //TODO: Remove (for debugging)
		sem_wait(&bd->sem);
	} else if (msgLen >= lenToBufEnd) {
		int bytesRemaining = msgLen - lenToBufEnd;
		if (bytesRemaining >= lastRead) {
			/* Wrap around will over shot past lastRead */
			/* Shouldn't happen - increase buffer size */
			__atomic_store_n(&bd->isWaiting, true, __ATOMIC_SEQ_CST);
			++wrap; //TODO: Remove (for debugging)
			sem_wait(&bd->sem);
		}
	}

	if (lenToBufEnd >= msgLen) {
		/* There is enough space at the end of the buffer */
		memcpy(bd->buf + lastWrite, msg, msgLen);
		__atomic_store_n(&bd->lastWrite, lastWrite + msgLen, __ATOMIC_SEQ_CST);
	} else {
		/* Space at the end of the buffer is insufficient - copy what is possible to
		 * the end, the rest copy at the beginning of the buffer */
		int bytesRemaining = msgLen - lenToBufEnd;
		memcpy(bd->buf + lastWrite, msg, lenToBufEnd);
		memcpy(bd->buf, msg + lenToBufEnd, bytesRemaining);
		__atomic_store_n(&bd->lastWrite, bytesRemaining, __ATOMIC_SEQ_CST);
	}

	return STATUS_SUCCESS;
}
