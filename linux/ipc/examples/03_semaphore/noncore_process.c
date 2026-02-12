/*
 * noncore_process.c — POSIX Named Semaphore: Consumer
 *
 * Demonstrates:
 *   - Opening an existing named semaphore
 *   - Blocking wait (sem_wait) until items are available
 *   - Reading items produced by another process
 *   - Cleanup: sem_close, sem_unlink, file removal
 *
 * Compile: gcc -Wall -Wextra -o noncore_process noncore_process.c -lpthread
 * Run:     ./noncore_process  (after or concurrently with core_process)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>

#define SEM_NAME   "/ipc_demo_sem"
#define DATA_FILE  "/tmp/ipc_sem_data.bin"
#define ITEM_COUNT 8

struct item {
	int  id;
	char description[64];
};

int
main(void)
{
	sem_t *sem;
	int fd;
	struct item it;
	int i;
	ssize_t n;

	/*
	 * sem_open() without O_CREAT — open an existing semaphore.
	 *
	 * The kernel looks up "/ipc_demo_sem" in the tmpfs
	 * at /dev/shm, and maps the same futex word into
	 * our address space.
	 *
	 * If the semaphore doesn't exist yet, returns SEM_FAILED
	 * with errno=ENOENT.  We retry for the producer to start.
	 */
	printf("[consumer] PID=%d, opening semaphore \"%s\"...\n",
	       getpid(), SEM_NAME);

	for(;;) {
		sem = sem_open(SEM_NAME, 0);
		if(sem != SEM_FAILED)
			break;
		if(errno != ENOENT) {
			perror("sem_open");
			exit(EXIT_FAILURE);
		}
		usleep(100000); /* wait for producer to create it */
	}

	printf("[consumer] semaphore opened\n");

	/* Wait for the data file to appear */
	while(access(DATA_FILE, F_OK) == -1)
		usleep(50000);

	fd = open(DATA_FILE, O_RDONLY);
	if(fd == -1) {
		perror("open");
		sem_close(sem);
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < ITEM_COUNT; i++) {
		/*
		 * sem_wait() — decrement the semaphore counter.
		 *
		 * If counter > 0:
		 *   Atomically decrements and returns immediately.
		 *
		 * If counter == 0:
		 *   The thread enters the wait queue via
		 *   futex(FUTEX_WAIT).  The kernel puts the task
		 *   in TASK_INTERRUPTIBLE state.  When the
		 *   producer calls sem_post(), FUTEX_WAKE
		 *   transitions us back to TASK_RUNNING.
		 *
		 * sem_wait() can be interrupted by a signal,
		 * in which case it returns -1 with errno=EINTR.
		 * Robust code must handle this.
		 *
		 * Alternative: sem_trywait() — non-blocking.
		 *   Returns -1/EAGAIN if counter is 0.
		 *
		 * Alternative: sem_timedwait() — blocks with timeout.
		 */
		if(sem_wait(sem) == -1) {
			if(errno == EINTR) {
				i--; /* retry */
				continue;
			}
			perror("sem_wait");
			break;
		}

		/* An item is available — read it */
		n = read(fd, &it, sizeof(it));
		if(n != sizeof(it)) {
			fprintf(stderr,
			        "[consumer] short read: %zd (expected %zu)\n",
			        n, sizeof(it));
			break;
		}

		printf("[consumer] consumed: id=%d desc=\"%s\"\n",
		       it.id, it.description);

		/* Simulate variable consumption time */
		usleep(50000 + (rand() % 100000)); /* 50-150ms */
	}

	close(fd);

	/*
	 * Cleanup:
	 *
	 * sem_close() — release our handle.
	 * sem_unlink() — remove the named semaphore from /dev/shm.
	 *   The name is removed immediately, but the semaphore
	 *   itself persists until all processes that have it
	 *   open call sem_close() (reference counting).
	 *
	 * If we crash without sem_unlink(), the semaphore file
	 * lingers in /dev/shm.  In production, clean up with:
	 *   rm /dev/shm/sem.ipc_demo_sem
	 */
	sem_close(sem);
	sem_unlink(SEM_NAME);
	unlink(DATA_FILE);

	printf("[consumer] done, cleaned up\n");

	return 0;
}
