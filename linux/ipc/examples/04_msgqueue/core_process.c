/*
 * core_process.c — POSIX Message Queue: Receiver
 *
 * Demonstrates:
 *   - Creating a POSIX message queue
 *   - Setting queue attributes (max messages, max size)
 *   - Blocking receive with priority
 *   - Cleanup with mq_close and mq_unlink
 *
 * POSIX message queues are backed by a special filesystem
 * (mqueue) typically mounted at /dev/mqueue/.  Each queue
 * is a kernel object containing:
 *   - A sorted list of messages (by priority, then FIFO)
 *   - A wait queue for blocked senders and receivers
 *   - Configurable limits (mq_maxmsg, mq_msgsize)
 *
 * System-wide limits:
 *   /proc/sys/fs/mqueue/msg_max      — max messages per queue
 *   /proc/sys/fs/mqueue/msgsize_max  — max bytes per message
 *   /proc/sys/fs/mqueue/queues_max   — max queues system-wide
 *
 * Compile: gcc -Wall -Wextra -o core_process core_process.c -lrt
 * Run:     ./core_process
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <errno.h>
#include <fcntl.h>

#define QUEUE_NAME  "/ipc_demo_mq"
#define MAX_MSGS    10
#define MAX_MSG_SZ  256
#define EXPECTED_MSGS 6

int
main(void)
{
	mqd_t mq;
	struct mq_attr attr;
	char buf[MAX_MSG_SZ];
	unsigned int prio;
	ssize_t n;
	int i;

	/* Remove stale queue */
	mq_unlink(QUEUE_NAME);

	/*
	 * Queue attributes control kernel resource allocation.
	 *
	 * mq_maxmsg:  max number of messages in the queue.
	 *   When the queue is full, mq_send() blocks (or
	 *   returns EAGAIN in non-blocking mode).
	 *   Kernel allocates this many message slots upfront.
	 *
	 * mq_msgsize: max size of a single message in bytes.
	 *   mq_send() fails with EMSGSIZE if you exceed this.
	 *   Kernel allocates slots of this size.
	 *
	 * Memory = mq_maxmsg * mq_msgsize (roughly).
	 * This is pinned kernel memory — can be limited by
	 * RLIMIT_MSGQUEUE (getrlimit/setrlimit).
	 */
	memset(&attr, 0, sizeof(attr));
	attr.mq_maxmsg  = MAX_MSGS;
	attr.mq_msgsize = MAX_MSG_SZ;

	/*
	 * mq_open() — create the message queue.
	 *
	 * O_CREAT | O_RDONLY: create if absent, open for receiving.
	 * Mode 0644: permissions.
	 *
	 * Internally, the kernel creates an inode in the mqueue
	 * filesystem and allocates the message array + wait queues.
	 *
	 * The returned mqd_t is actually a file descriptor (on Linux).
	 * You can use it with select()/poll()/epoll() for non-blocking
	 * notification.
	 */
	mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
	if(mq == (mqd_t)-1) {
		perror("mq_open");
		exit(EXIT_FAILURE);
	}

	printf("[receiver] PID=%d, queue \"%s\" created\n",
	       getpid(), QUEUE_NAME);
	printf("[receiver] waiting for %d messages...\n", EXPECTED_MSGS);

	for(i = 0; i < EXPECTED_MSGS; i++) {
		/*
		 * mq_receive() — receive the highest-priority message.
		 *
		 * Behavior:
		 *   1. If queue is non-empty: dequeue the message
		 *      with the highest priority (highest unsigned int
		 *      value).  Among equal priority, FIFO order.
		 *
		 *   2. If queue is empty: block in the wait queue
		 *      until a message arrives.  The task enters
		 *      TASK_INTERRUPTIBLE state.
		 *
		 * Parameters:
		 *   mq   — queue descriptor
		 *   buf  — receive buffer (must be >= mq_msgsize)
		 *   MAX_MSG_SZ — buffer size
		 *   &prio — output: priority of received message
		 *
		 * Returns: number of bytes in the message (not counting
		 * any null terminator — messages are binary blobs).
		 *
		 * IMPORTANT: buf MUST be at least mq_msgsize bytes.
		 * If smaller, mq_receive() fails with EMSGSIZE.
		 */
		n = mq_receive(mq, buf, MAX_MSG_SZ, &prio);
		if(n == -1) {
			perror("mq_receive");
			break;
		}

		/* Null-terminate for safe printing (we sent text) */
		buf[n] = '\0';

		printf("[receiver] msg %d: prio=%u size=%zd data=\"%s\"\n",
		       i, prio, n, buf);
	}

	/*
	 * mq_close() — close our descriptor.  The queue persists.
	 * mq_unlink() — remove the queue name from /dev/mqueue.
	 *   The queue is destroyed when all descriptors are closed
	 *   AND the name is unlinked (same semantics as unlink(2)
	 *   for regular files).
	 */
	mq_close(mq);
	mq_unlink(QUEUE_NAME);

	printf("[receiver] done, queue removed\n");

	return 0;
}
