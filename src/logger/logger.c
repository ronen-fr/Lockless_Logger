/*
 ============================================================================
 Name        : Logger_C.c
 Author      : Barak Sason Rofman
 Version     :
 Copyright   : Your copyright notice
 Description :
 ============================================================================
 */

#include "logger.h"

enum statusCodes {
	STATUS_FAILURE = -1, STATUS_SUCCESS
};

typedef struct sharedBuffer {
	int lastRead;
	int lastWrite;
	int bufSize;
	char* buf;
	pthread_mutex_t lock;
} sharedBuffer;

static bool isTerminate;
static int privateBufferSize;
static int sharedBufferSize;
static FILE* logFile;
static int fileHandle;
static int bufferDataArraySize;
static int nextFreeCell;
static bufferData** bufferDataArray;
static pthread_mutex_t loggerLock;
static pthread_t loggerThread;
static sharedBuffer sharedBuf;

static void initBufferData(bufferData* bd);
static int createLogFile();
static void* runLogger();

inline static bufferData* getPrivateBuffer(pid_t tid);
inline static bool isNextWriteOverwrite(const int msgLen, const int lastRead,
										const atomic_int lastWrite,
										const int lenToBufEnd);
inline static bool isSequentialOverwrite(const int lastRead,
											const int lastWrite,
											const int msgLen);
inline static bool isWraparoundOverwrite(const int lastRead, const int msgLen,
											const int lenToBufEnd);
inline static int writeToPrivateBuffer(bufferData* bd, const char* msg,
										const int msgLen);
inline static int writeSeq(char* buf, const char* msg, const int msgLen,
							const int lastWrite);
inline static int writeWrap(char* buf, const char* msg, const int msgLen,
							const int lastWrite, const int lenToBufEnd);
inline static int writeToSharedBuffer(const char* msg, const int msgLen);
inline static int writeToFile(char* buf, const int lastRead,
								const int lastWrite, const int bufSize);
inline static void drainPrivateBuffers();
inline static void drainSharedBuffer();

/* Initialize all data required by the logger */
int initLogger(const int threadsNum, int privateBuffSize, int sharedBuffSize) {
	int i;

	if (0 >= threadsNum || 0 >= privateBuffSize || 0 >= sharedBuffSize) {
		return STATUS_FAILURE;
	}

	bufferDataArraySize = threadsNum;
	privateBufferSize = privateBuffSize;
	sharedBufferSize = sharedBuffSize;

	pthread_mutex_init(&loggerLock, NULL);

	if (STATUS_SUCCESS != createLogFile()) {
		return STATUS_FAILURE;
	}

	/* Init private buffers */
	//TODO: think if malloc failures need to be handled
	bufferDataArray = malloc(threadsNum * sizeof(bufferData*));
	for (i = 0; i < bufferDataArraySize; ++i) {
		bufferDataArray[i] = malloc(sizeof(bufferData));
		initBufferData(bufferDataArray[i]);
	}

	/* Init shared buffer */
	sharedBuf.bufSize = sharedBufferSize;
	sharedBuf.buf = malloc(sharedBufferSize * sizeof(char));
	sharedBuf.lastWrite = 1;
	pthread_mutex_init(&sharedBuf.lock, NULL);

	/* Run logger thread */
	pthread_create(&loggerThread, NULL, runLogger, NULL);

	return STATUS_SUCCESS;
}

/* Initialize given bufferData struct */
void initBufferData(bufferData* bd) {
	bd->bufSize = privateBufferSize;
	//TODO: think if malloc failures need to be handled
	bd->buf = malloc(privateBufferSize);
	bd->lastWrite = 1;
}

/* Create log file */
static int createLogFile() {
	//TODO: implement rotating log
	logFile = fopen("logFile.txt", "w");

	if (NULL == logFile) {
		//TODO: handle error
		return STATUS_FAILURE;
	}

	fileHandle = fileno(logFile);

	return STATUS_SUCCESS;
}

/* Register a worker thread at the logger and assign one of the buffers to it */
int registerThread(pid_t tid) {
	pthread_mutex_lock(&loggerLock); /* Lock */
	{
		if (bufferDataArraySize == nextFreeCell) {
			return STATUS_FAILURE;
		}
		bufferDataArray[nextFreeCell++]->tid = tid;
	}
	pthread_mutex_unlock(&loggerLock); /* Unlock */

	return STATUS_SUCCESS;
}

/* Logger thread loop */
static void* runLogger() {
	while (false == isTerminate) {
		drainPrivateBuffers();
		drainSharedBuffer();
		/* Flush the stream in case worker threads wrote to it */
		fflush(logFile);
	}

	return NULL;
}

/* Drain all private buffers assigned to worker threads to the log file */
inline static void drainPrivateBuffers() {
	int i;
	int newLastRead;
	atomic_int lastWrite;
	bufferData* bd;

	for (i = 0; i < bufferDataArraySize; ++i) {
		bd = bufferDataArray[i];
		if (NULL != bd) {
			/* Atomic load lastWrite, as it is changed by the worker thread */
			__atomic_load(&bd->lastWrite, &lastWrite, __ATOMIC_SEQ_CST);

			newLastRead = writeToFile(bd->buf, bd->lastRead, lastWrite,
										bd->bufSize);
			if (STATUS_FAILURE != newLastRead) {
				/* Atomic store lastRead, as it is read by the worker thread */
				__atomic_store_n(&bd->lastRead, newLastRead,
				__ATOMIC_SEQ_CST);
			}
		}
	}
}

/* Drain the buffer which is shared by the worker threads to the log file */
inline static void drainSharedBuffer() {
	int newLastRead;

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

/* Perform actual write to log file from a given buffer.
 * In a case of successful write, the method returns the new position of lastRead.
 * In case of failure, the method returns -1. */
inline static int writeToFile(char* buf, const int lastRead,
								const int lastWrite, const int bufSize) {
	int dataLen;
	int lenToBufEnd;

	if (lastWrite > lastRead) {
		dataLen = lastWrite - lastRead - 1;

		if (dataLen > 0) {
			/* Sequential write */
			write(fileHandle, buf + lastRead + 1, dataLen);

			return lastWrite - 1;
		}
	} else {
		lenToBufEnd = bufSize - lastRead;
		dataLen = lenToBufEnd + lastWrite - 1;

		if (dataLen > 0) {
			/* Wrap around write */
			write(fileHandle, buf + lastRead + 1, lenToBufEnd);
			write(fileHandle, buf, lastWrite);

			return lastWrite - 1;
		}
	}

	return STATUS_FAILURE;
}

/* Terminate the logger thread and release resources */
void terminateLogger() {
	isTerminate = true;
	pthread_join(loggerThread, NULL);
	fclose(logFile);
}

/* Get a buffer assigned to a worker thread based on thread id */
inline static bufferData* getPrivateBuffer(pid_t tid) {
	int i;
	bufferData* bd;

	for (i = 0; i < nextFreeCell; ++i) {
		bd = bufferDataArray[i];
		if (bd->tid == tid) {
			return bd;
		}
	}

	return NULL;
}

/* Add a message from a worker thread to a buffer or write it directly to file
 * if buffers are full.
 * Note: 'msg' must be a null-terminated string */
int logMessage(const char* msg) {
	int msgLen;
	bufferData* bd;

	if (NULL == msg) {
		return STATUS_FAILURE;
	}

	//TODO: implement rotating file write
	bd = getPrivateBuffer(getpid());
	if (NULL == bd) {
		//TODO: think if write from unregistered worker threads should be allowed
		return STATUS_FAILURE;
	}

	msgLen = strlen(msg);

	if (STATUS_SUCCESS != writeToPrivateBuffer(bd, msg, msgLen)) {
		if (STATUS_SUCCESS != writeToSharedBuffer(msg, msgLen)) {
			//TODO: remove, for debug only
			++cnt;

			fwrite(msg, 1, msgLen, logFile);
			/* Flushing of the stream is performed by the logger thread */
		}
	}

	return STATUS_SUCCESS;
}

/* Add a message from a worker thread to it's private buffer */
inline static int writeToPrivateBuffer(bufferData* bd, const char* msg,
										const int msgLen) {
	int lastRead;
	int lastWrite;
	int lenToBufEnd;
	int newLastWrite;

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
inline static bool isNextWriteOverwrite(const int msgLen, const int lastRead,
										const atomic_int lastWrite,
										const int lenToBufEnd) {
	return (isSequentialOverwrite(lastRead, lastWrite, msgLen)
			|| isWraparoundOverwrite(lastRead, msgLen, lenToBufEnd));
}

/* Check for sequential data override */
inline static bool isSequentialOverwrite(const int lastRead,
											const int lastWrite,
											const int msgLen) {
	return (lastWrite < lastRead && ((lastWrite + msgLen) >= lastRead));
}

/* Check for wrap-around data override */
inline static bool isWraparoundOverwrite(const int lastRead, const int msgLen,
											const int lenToBufEnd) {
	if (msgLen > lenToBufEnd) {
		int bytesRemaining = msgLen - lenToBufEnd;
		return bytesRemaining >= lastRead;
	}

	return false;
}

/* Write sequentially to buffer */
inline static int writeSeq(char* buf, const char* msg, const int msgLen,
							const int lastWrite) {
	memcpy(buf + lastWrite, msg, msgLen);

	return lastWrite + msgLen;
}

/* Space at the end of the buffer is insufficient - write what is possible to
 * the end of the buffer, the rest write at the beginning of the buffer */
inline static int writeWrap(char* buf, const char* msg, const int msgLen,
							const int lastWrite, const int lenToBufEnd) {
	int bytesRemaining = msgLen - lenToBufEnd;
	memcpy(buf + lastWrite, msg, lenToBufEnd);
	memcpy(buf, msg + lenToBufEnd, bytesRemaining);

	return bytesRemaining;
}

/* Add a message from a worker thread to the shared buffer */
inline static int writeToSharedBuffer(const char* msg, const int msgLen) {
	int res = STATUS_FAILURE;
	int lastRead;
	int lastWrite;
	int lenToBufEnd;
	int newLastWrite;

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
			res = STATUS_SUCCESS;
		}
	}
	pthread_mutex_unlock(&sharedBuf.lock); /* Unlock */

	return res;
}
