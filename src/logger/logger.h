/*
 ============================================================================
 Name        : logger.h
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#ifndef LOGGER
#define LOGGER

#include <pthread.h>
#include <stdatomic.h>
#include <sys/time.h>

enum loggerStatusCodes {
	STATUS_LOGGER_FAILURE = -1, STATUS_LOGGER_SUCCESS
};

enum logLevels {
	LOG_LEVEL_NONE, /* No logging */ // consider reversing the order of the rest
	LOG_LEVEL_EMERG, LOG_LEVEL_ALERT, LOG_LEVEL_CRITICAL, /* Fatal failure */
	LOG_LEVEL_ERROR, /* Non-fatal failure */
	LOG_LEVEL_WARNING, /* Attention required for normal operations */
	LOG_LEVEL_NOTICE, LOG_LEVEL_INFO, /* Normal information */
	LOG_LEVEL_DEBUG, /* Internal errors */
	LOG_LEVEL_TRACE, /* Code-flow tracing */
};

//TODO: remove, for debug only
long long cnt;

/*
// a comment from SO:
If you have 3 separate atomic objects, don't put them all in the same struct if you want to use them from different threads. Having them all in one cache line will cause false sharing, so threads using counter1 will contend with threads using counter2. (If they're usually all used at the same time, then in the same cache line is good, though.) – Peter Cordes May 30 '18 at 10:05
*/
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
	int millisec;  // you are mixing milliseconds and microseconds!
	pthread_t tid;
	struct tm* tm_info;
	struct timeval tv;
	int logLevel;	// how does one change the log level after init?
	int loggingMethod;
} messageInfo;

int initLogger(const int threadsNum, int privateBuffSize, int sharedBuffSize,
               int loggingLevel); // threadsNum must be known in advance?
int registerThread(pthread_t tid);

/* 'logMessage' should be called only by using the macro 'LOG_LEVEL_MSG' */
int logMessage(int loggingLevel, char* file, const int line, const char* func,
               const char* msg, ...);

void terminateLogger();

#define LOG_MSG(loggingLevel, msg ...) logMessage(loggingLevel, __FILE__, __LINE__ ,__PRETTY_FUNCTION__ , msg)

#endif /* LOGGER */
