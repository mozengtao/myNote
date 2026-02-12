/*
 * noncore_process.c — POSIX Shared Memory + Semaphore: Consumer
 *
 * Demonstrates:
 *   - Opening existing shared memory object
 *   - mmap into our address space (zero-copy read)
 *   - Semaphore-based consumer in a bounded ring buffer
 *   - Full cleanup of all IPC resources
 *
 * Compile: gcc -Wall -Wextra -o noncore_process noncore_process.c -lrt -lpthread
 * Run:     ./noncore_process  (after or concurrently with core_process)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <errno.h>

#define SHM_NAME    "/ipc_demo_shm"
#define SEM_EMPTY   "/ipc_shm_empty"
#define SEM_FULL    "/ipc_shm_full"

#define RING_SIZE   4
#define TOTAL_ITEMS 12

struct shm_ring {
	_Atomic int write_idx;
	_Atomic int read_idx;
	int         done;
	struct {
		int  id;
		char data[60];
	} slots[RING_SIZE];
};

int
main(void)
{
	int shm_fd;
	struct shm_ring *ring;
	sem_t *sem_empty, *sem_full;
	int count;

	printf("[consumer] PID=%d, waiting for shared memory...\n",
	       getpid());

	/* Wait for the shared memory object to be created */
	for(;;) {
		shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
		if(shm_fd != -1)
			break;
		if(errno != ENOENT) {
			perror("shm_open");
			exit(EXIT_FAILURE);
		}
		usleep(100000);
	}

	/*
	 * mmap() the same shared memory object.
	 *
	 * The kernel reuses the SAME physical pages that the
	 * producer mapped.  After this, writes by the producer
	 * to ring->slots[i] are visible to us (modulo memory
	 * ordering — hence the atomic operations and barriers).
	 *
	 *  Process A (producer)          Process B (consumer)
	 *  +------------------+          +------------------+
	 *  | Virtual Addr 0x7f|          | Virtual Addr 0x7a|
	 *  |   ring->slots[0] |          |   ring->slots[0] |
	 *  +--------+---------+          +---------+--------+
	 *           |                              |
	 *           +------+  Physical  +----------+
	 *                  |   Page     |
	 *                  v            v
	 *             +--------------------+
	 *             | Physical RAM Frame |
	 *             | (same page!)       |
	 *             +--------------------+
	 *
	 * No memcpy, no read/write syscall, no pipe — just direct
	 * memory access through the MMU page tables.
	 */
	ring = mmap(NULL, sizeof(struct shm_ring),
	            PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if(ring == MAP_FAILED) {
		perror("mmap");
		close(shm_fd);
		exit(EXIT_FAILURE);
	}
	close(shm_fd);

	/* Open existing semaphores (retry until producer creates them) */
	for(;;) {
		sem_empty = sem_open(SEM_EMPTY, 0);
		sem_full  = sem_open(SEM_FULL, 0);
		if(sem_empty != SEM_FAILED && sem_full != SEM_FAILED)
			break;
		usleep(50000);
	}

	printf("[consumer] attached to shared memory and semaphores\n");

	count = 0;
	for(;;) {
		/*
		 * Wait for a full slot.
		 *
		 * SEM_FULL tracks how many slots contain data
		 * ready for consumption.  If 0, we block.
		 */
		sem_wait(sem_full);

		/* Check if producer signaled completion */
		if(ring->done &&
		   atomic_load(&ring->read_idx) >=
		   atomic_load(&ring->write_idx))
			break;

		/*
		 * Read directly from shared memory — zero copy.
		 *
		 * atomic_load with memory_order_acquire ensures
		 * we see all stores that happened before the
		 * producer's release store to write_idx.
		 */
		int idx = atomic_load(&ring->read_idx) % RING_SIZE;

		printf("[consumer] read slot[%d]: id=%d data=\"%s\"\n",
		       idx, ring->slots[idx].id, ring->slots[idx].data);

		atomic_store_explicit(&ring->read_idx,
		                      atomic_load(&ring->read_idx) + 1,
		                      memory_order_release);

		count++;

		/* Signal: one more slot available for writing */
		sem_post(sem_empty);

		usleep(120000); /* 120ms — slower consumer */
	}

	printf("[consumer] consumed %d items total\n", count);

	/*
	 * Full cleanup — remove all IPC objects:
	 *
	 * munmap():      release our mapping (VMA removed)
	 * shm_unlink():  remove /dev/shm/ipc_demo_shm
	 *   The physical pages are freed when no more mappings
	 *   reference them.
	 *
	 * sem_close():   release our semaphore handles
	 * sem_unlink():  remove /dev/shm/sem.ipc_shm_*
	 */
	munmap(ring, sizeof(struct shm_ring));
	shm_unlink(SHM_NAME);
	sem_close(sem_empty);
	sem_close(sem_full);
	sem_unlink(SEM_EMPTY);
	sem_unlink(SEM_FULL);

	printf("[consumer] cleaned up all IPC resources\n");

	return 0;
}
