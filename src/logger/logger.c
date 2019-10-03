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

enum statusCodes {
	STATUS_FAILURE = -1, STATUS_SUCCESS
};

typedef struct sharedBuffer {
	unsigned int lastRead;
	unsigned int lastWrite;
	unsigned int bufSize;
	char *buf;
	pthread_mutex_t lock;
} sharedBuffer;

static bool isTerminate;
static int logFile;
static unsigned int bufferDataArraySize;
static unsigned int nextFreeCell;
static bufferData **bufferDataArray;
static pthread_mutex_t lock;
static pthread_t loggerThread;
static sharedBuffer sharedBuf;

static int createLogFile();
static void* runLogger();

inline static bufferData* getPrivateBuffer();
inline static bool isNextWriteOverwrite(const unsigned int msgLen,
		const unsigned int lastRead, const atomic_uint lastWrite,
		const unsigned int lenToBufEnd);
inline static bool isSequentialOverwrite(const unsigned int lastRead,
		const unsigned int lastWrite, const unsigned int msgLen);
inline static bool isWraparoundOverwrite(const unsigned int lastRead,
		const unsigned int msgLen, const unsigned int lenToBufEnd);
inline static int writeToPrivateBuffer(bufferData *bd, const char *msg,
		const unsigned int msgLen);
inline static unsigned int writeSeq(char *buf, const char *msg,
		const unsigned int msgLen, const unsigned int lastWrite);
inline static unsigned int writeWrap(char *buf, const char *msg,
		const unsigned int msgLen, const unsigned int lastWrite,
		const unsigned int lenToBufEnd);
inline static int writeToSharedBuffer(const char *msg,
		const unsigned int msgLen);
inline static long writeToFile(char *buf, const unsigned int lastRead,
		const unsigned int lastWrite, const unsigned int bufSize);
inline static void drainPrivateBuffers();
inline static void drainSharedBuffer();

/* Initialize given bufferData struct */
void initBufferData(bufferData *bd) {
	bd->bufSize = BUFFSIZE;
	//TODO: think if malloc failures need to be handled
	bd->buf = malloc(BUFFSIZE);
	bd->lastWrite = 1;
}

/* Initialize all data required by the logger */
int initLogger(const unsigned int threadsNum) {
	unsigned int i;

	bufferDataArraySize = threadsNum;
	pthread_mutex_init(&lock, NULL);

	if (STATUS_SUCCESS != createLogFile()) {
		return STATUS_FAILURE;
	}

	//TODO: think if malloc failures need to be handled
	bufferDataArray = malloc(threadsNum * sizeof(bufferData*));
	for (i = 0; i < bufferDataArraySize; ++i) {
		bufferDataArray[i] = malloc(sizeof(bufferData));
		initBufferData(bufferDataArray[i]);
	}

	/* Init shared buffer */
	sharedBuf.bufSize = SHAREDBUFFSIZE;
	sharedBuf.buf = malloc(SHAREDBUFFSIZE * sizeof(char));
	sharedBuf.lastWrite = 1;
	pthread_mutex_init(&sharedBuf.lock, NULL);
	pthread_create(&loggerThread, NULL, runLogger, NULL);

	return STATUS_SUCCESS;
}

/* Create log file */
static int createLogFile() {
	//TODO: implement rotating log
	logFile = open("logFile.txt", O_RDWR | O_CREAT | O_TRUNC, 0777);
	if (-1 == logFile) {
		//TODO: handle error
		return STATUS_FAILURE;
	}
	return STATUS_SUCCESS;
}

/* Register a worker thread at the logger and assign one of the buffers to it */
int registerThread() {
	pthread_mutex_lock(&lock); /* Lock */
	{
		if (bufferDataArraySize == nextFreeCell) {
			//TODO: handle error
			return STATUS_FAILURE;
		}
		bufferDataArray[nextFreeCell++]->tid = pthread_self();
	}
	pthread_mutex_unlock(&lock); /* Unlock */

	return STATUS_SUCCESS;
}

/* Logger thread loop */
static void* runLogger() {
	while (false == isTerminate) {
		drainPrivateBuffers();
		drainSharedBuffer();
	}

	return NULL;
}

/* Drain all private buffers assigned to worker threads to the log file */
inline static void drainPrivateBuffers() {
	unsigned int i;
	unsigned int newLastRead;
	atomic_uint lastWrite;
	bufferData *bd;

	for (i = 0; i < bufferDataArraySize; ++i) {
		bd = bufferDataArray[i];
		if (NULL != bd) {
			/* Atomic load lastWrite, as it is changed by the worker thread */
			__atomic_load(&bd->lastWrite, &lastWrite, __ATOMIC_SEQ_CST);

			newLastRead = writeToFile(bd->buf, bd->lastRead, lastWrite,
					bd->bufSize);
			if (STATUS_FAILURE != newLastRead) {
				/* Atomic store lastRead, as it is read by the worker thread */
				__atomic_store_n(&bd->lastRead, newLastRead, __ATOMIC_SEQ_CST);
			}
		}
	}
}

/* Drain the buffer which is shared by the worker threads to the log file */
inline static void drainSharedBuffer() {
	unsigned int newLastRead;

	pthread_mutex_lock(&sharedBuf.lock); /* Lock */
	{
		newLastRead = writeToFile(sharedBuf.buf, sharedBuf.lastRead,
				sharedBuf.lastWrite, sharedBuf.bufSize);
		if (STATUS_FAILURE != newLastRead) {
			sharedBuf.lastRead = newLastRead;
		}
	}
	pthread_mutex_unlock(&sharedBuf.lock); /* Unlock */
}

/* Perform actual write to log file from a given buffer */
inline static long writeToFile(char *buf, const unsigned int lastRead,
		const unsigned int lastWrite, const unsigned int bufSize) {
	unsigned int dataLen;
	unsigned int lenToBufEnd;

	if (lastWrite > lastRead) {
		dataLen = lastWrite - lastRead - 1;

		if (dataLen > 0) {
			/* Sequential write */
			write(logFile, buf + lastRead, dataLen);

			return lastWrite - 1;
		}
	} else {
		lenToBufEnd = bufSize - lastRead;
		dataLen = lenToBufEnd + lastWrite - 1;

		if (dataLen > 0) {
			/* Wrap around write */
			write(logFile, buf + lastRead, lenToBufEnd);
			write(logFile, buf, lastWrite);

			return lastWrite - 1;
		}
	}

	return STATUS_FAILURE;
}

/* Terminate the logger thread and release resources */
void terminateLogger() {
	isTerminate = true;
	pthread_join(loggerThread, NULL);
	close(logFile);
}

/* Get a buffer assigned to a worker thread based on thread id */
inline static bufferData* getPrivateBuffer() {
	unsigned int i;
	bufferData *bd;
	pthread_t thrd = pthread_self();

	for (i = 0; i < nextFreeCell; ++i) {
		bd = bufferDataArray[i];
		if (0 != pthread_equal(bd->tid, thrd)) {
			return bd;
		}
	}

	return NULL;
}

/* Add a message from a worker thread to a buffer or write it directly to file
 * if buffers are full */
int logMessage(const char *msg) {
	unsigned int msgLen;
	bufferData *bd;

	//TODO: implement rotating file write
	bd = getPrivateBuffer();
	if (NULL == bd) {
		//TODO: think if write from unregistered worker threads should be allowed
		return STATUS_FAILURE;
	}

	msgLen = strlen(msg);

	if (STATUS_SUCCESS != writeToPrivateBuffer(bd, msg, msgLen)) {
		if (STATUS_SUCCESS != writeToSharedBuffer(msg, msgLen)) {
			printf("Shared buffer full\n");
		}
	}

	return STATUS_SUCCESS;
}

/* Add a message from a worker thread to it's private buffer */
inline static int writeToPrivateBuffer(bufferData *bd, const char *msg,
		const unsigned int msgLen) {
	unsigned int lastRead;
	unsigned int lastWrite;
	unsigned int lenToBufEnd;
	unsigned int newLastWrite;

	/* Atomic load lastRead, as it is changed by the logger thread */
	__atomic_load(&bd->lastRead, &lastRead, __ATOMIC_SEQ_CST);

	lastWrite = bd->lastWrite;
	lenToBufEnd = bd->bufSize - lastWrite;

	if (false
			== isNextWriteOverwrite(msgLen, lastRead, lastWrite, lenToBufEnd)) {
		newLastWrite =
				lenToBufEnd >= msgLen ?
						writeSeq(bd->buf, msg, msgLen, lastWrite) :
						writeWrap(bd->buf, msg, msgLen, lastWrite, lenToBufEnd);

		/* Atomic store lastWrite, as it is read by the logger thread */
		__atomic_store_n(&bd->lastWrite, newLastWrite, __ATOMIC_SEQ_CST);
		return STATUS_SUCCESS;
	}

	return STATUS_FAILURE;
}

/* Check for 2 potential cases data override */
inline static bool isNextWriteOverwrite(const unsigned int msgLen,
		const unsigned int lastRead, const atomic_uint lastWrite,
		const unsigned int lenToBufEnd) {
	return (isSequentialOverwrite(lastRead, lastWrite, msgLen)
			|| isWraparoundOverwrite(lastRead, msgLen, lenToBufEnd));
}

/* Check for sequential data override */
inline static bool isSequentialOverwrite(const unsigned int lastRead,
		const unsigned int lastWrite, const unsigned int msgLen) {
	return (lastWrite < lastRead && ((lastWrite + msgLen) >= lastRead));
}

/* Check for wrap-around data override */
inline static bool isWraparoundOverwrite(const unsigned int lastRead,
		const unsigned int msgLen, const unsigned int lenToBufEnd) {
	if (msgLen > lenToBufEnd) {
		int bytesRemaining = msgLen - lenToBufEnd;
		return bytesRemaining >= lastRead;
	}

	return false;
}

/* Write sequentially to buffer */
inline static unsigned int writeSeq(char *buf, const char *msg,
		const unsigned int msgLen, const unsigned int lastWrite) {
	memcpy(buf + lastWrite, msg, msgLen);

	return lastWrite + msgLen;
}

/* Space at the end of the buffer is insufficient - write what is possible to
 * the end of the buffer, the rest write at the beginning of the buffer */
inline static unsigned int writeWrap(char *buf, const char *msg,
		const unsigned int msgLen, const unsigned int lastWrite,
		const unsigned int lenToBufEnd) {
	unsigned int bytesRemaining = msgLen - lenToBufEnd;
	memcpy(buf + lastWrite, msg, lenToBufEnd);
	memcpy(buf, msg + lenToBufEnd, bytesRemaining);

	return bytesRemaining;
}

/* Add a message from a worker thread to the shared buffer */
inline static int writeToSharedBuffer(const char *msg,
		const unsigned int msgLen) {
	unsigned int lastRead;
	unsigned int lastWrite;
	unsigned int lenToBufEnd;
	unsigned int newLastWrite;

	pthread_mutex_lock(&sharedBuf.lock); /* Lock */
	{
		lastRead = sharedBuf.lastRead;
		lastWrite = sharedBuf.lastWrite;
		lenToBufEnd = sharedBuf.bufSize - sharedBuf.lastWrite;

		if (false
				== isNextWriteOverwrite(msgLen, lastRead, lastWrite,
						lenToBufEnd)) {
			newLastWrite =
					lenToBufEnd >= msgLen ?
							writeSeq(sharedBuf.buf, msg, msgLen, lastWrite) :
							writeWrap(sharedBuf.buf, msg, msgLen, lastWrite,
									lenToBufEnd);

			sharedBuf.lastWrite = newLastWrite;
			pthread_mutex_unlock(&sharedBuf.lock);
			return STATUS_SUCCESS;
		}
	}
	pthread_mutex_unlock(&sharedBuf.lock); /* Unlock */
	return STATUS_FAILURE;
}
