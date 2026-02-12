/*
 * core_process.c — POSIX Named Semaphore: Producer
 *
 * Demonstrates:
 *   - Creating a POSIX named semaphore (sem_open)
 *   - Using a shared file as a "slot" buffer
 *   - Producer posts (increments) the semaphore after writing
 *   - Proper cleanup with sem_close and sem_unlink
 *
 * POSIX semaphores live in a tmpfs mounted at /dev/shm/.
 * A named semaphore "/my_sem" becomes /dev/shm/sem.my_sem.
 *
 * The kernel maintains a counter (atomic integer) plus a
 * wait queue.  sem_wait() decrements the counter; if it
 * would go negative, the calling thread sleeps on the
 * wait queue.  sem_post() increments the counter and
 * wakes one sleeper.
 *
 * Compile: gcc -Wall -Wextra -o core_process core_process.c -lpthread
 * Run:     ./core_process
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

/* Shared data structure — one item at a time */
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

	/* Clean up from previous runs */
	sem_unlink(SEM_NAME);
	unlink(DATA_FILE);

	/*
	 * sem_open() — create a new named semaphore.
	 *
	 * O_CREAT | O_EXCL: create new, fail if exists.
	 * Mode 0644: permissions (like files).
	 * Initial value 0: consumer must wait until producer posts.
	 *
	 * The semaphore is backed by a file in /dev/shm/ (tmpfs).
	 * The kernel maps it into the calling process's address
	 * space.  The sem_t* returned is a pointer to the mapped
	 * region containing the futex-based counter.
	 *
	 * Named semaphores persist until sem_unlink() removes them,
	 * even if all processes close their handles.
	 */
	sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 0);
	if(sem == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	printf("[producer] PID=%d, semaphore \"%s\" created (initial=0)\n",
	       getpid(), SEM_NAME);

	fd = open(DATA_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if(fd == -1) {
		perror("open");
		sem_close(sem);
		sem_unlink(SEM_NAME);
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < ITEM_COUNT; i++) {
		/* Produce an item */
		memset(&it, 0, sizeof(it));
		it.id = i;
		snprintf(it.description, sizeof(it.description),
		         "Item-%d produced by PID %d", i, getpid());

		/* Write to the shared file */
		if(write(fd, &it, sizeof(it)) != sizeof(it)) {
			perror("write");
			break;
		}
		fsync(fd);

		printf("[producer] produced item %d\n", i);

		/*
		 * sem_post() — increment the semaphore counter.
		 *
		 * Internally:
		 *   1. Atomically increments the counter (futex word)
		 *   2. If any threads are sleeping in sem_wait(),
		 *      calls futex(FUTEX_WAKE) to wake one
		 *   3. The woken thread returns from sem_wait()
		 *
		 * sem_post() never blocks — it always succeeds
		 * (unless the semaphore is invalid or the counter
		 * would overflow SEM_VALUE_MAX).
		 */
		if(sem_post(sem) == -1) {
			perror("sem_post");
			break;
		}

		/* Simulate variable production time */
		usleep(100000 + (rand() % 200000)); /* 100-300ms */
	}

	close(fd);

	/*
	 * sem_close() — detach from the semaphore.
	 * Unmaps the shared memory region.  The semaphore
	 * itself persists (it's named).
	 *
	 * We don't sem_unlink() here — let the consumer
	 * clean up after it finishes reading.
	 */
	sem_close(sem);

	printf("[producer] done, produced %d items\n", ITEM_COUNT);

	return 0;
}
