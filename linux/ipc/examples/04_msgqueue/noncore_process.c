/*
 * noncore_process.c — POSIX Message Queue: Sender
 *
 * Demonstrates:
 *   - Opening an existing message queue
 *   - Sending messages with different priorities
 *   - Queue-full behavior
 *
 * Compile: gcc -Wall -Wextra -o noncore_process noncore_process.c -lrt
 * Run:     ./noncore_process  (after starting core_process)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <errno.h>
#include <fcntl.h>

#define QUEUE_NAME  "/ipc_demo_mq"
#define MAX_MSG_SZ  256

/*
 * Messages with priorities.
 * Higher numeric priority = dequeued first.
 *
 * Real use cases:
 *   - Logging: prio 0=debug, 1=info, 2=warn, 3=error
 *   - Task queues: prio 0=background, 3=urgent
 */
struct msg_def {
	const char *text;
	unsigned int prio;
};

static struct msg_def messages[] = {
	{ "Low priority background task",     0 },
	{ "Normal log entry",                 1 },
	{ "Another normal message",           1 },
	{ "High priority alert",              3 },
	{ "Critical system event",            5 },
	{ "Medium priority notification",     2 },
};

#define MSG_COUNT (sizeof(messages) / sizeof(messages[0]))

int
main(void)
{
	mqd_t mq;
	unsigned int i;

	/*
	 * mq_open() without O_CREAT — open existing queue.
	 * O_WRONLY — we only send.
	 *
	 * If the queue doesn't exist yet (receiver hasn't started),
	 * we retry.
	 */
	printf("[sender] PID=%d, opening queue \"%s\"...\n",
	       getpid(), QUEUE_NAME);

	for(;;) {
		mq = mq_open(QUEUE_NAME, O_WRONLY);
		if(mq != (mqd_t)-1)
			break;
		if(errno != ENOENT) {
			perror("mq_open");
			exit(EXIT_FAILURE);
		}
		usleep(100000);
	}

	printf("[sender] queue opened, sending %zu messages\n", MSG_COUNT);

	for(i = 0; i < MSG_COUNT; i++) {
		/*
		 * mq_send() — send a message to the queue.
		 *
		 * Parameters:
		 *   mq    — queue descriptor
		 *   text  — message data (arbitrary bytes)
		 *   len   — message length (must be <= mq_msgsize)
		 *   prio  — message priority (unsigned int)
		 *
		 * The kernel inserts the message into the sorted list:
		 *   - First by priority (highest first)
		 *   - Then by arrival time (FIFO within same priority)
		 *
		 * Blocking behavior:
		 *   - If queue is not full: enqueue and return.
		 *   - If queue is full: block until space is available
		 *     (some receiver calls mq_receive()).
		 *
		 * Non-blocking alternative:
		 *   Open with O_NONBLOCK, then mq_send() returns -1
		 *   with errno=EAGAIN if the queue is full.
		 *
		 * Timed alternative:
		 *   mq_timedsend() — blocks with a timeout.
		 */
		if(mq_send(mq, messages[i].text,
		           strlen(messages[i].text),
		           messages[i].prio) == -1) {
			perror("mq_send");
			break;
		}

		printf("[sender] sent prio=%u: \"%s\"\n",
		       messages[i].prio, messages[i].text);

		usleep(100000); /* 100ms between sends */
	}

	mq_close(mq);

	printf("[sender] done\n");

	return 0;
}
