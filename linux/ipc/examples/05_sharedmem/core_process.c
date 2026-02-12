/*
 * core_process.c — POSIX Shared Memory + Semaphore: Producer
 *
 * Demonstrates:
 *   - Creating POSIX shared memory (shm_open + ftruncate + mmap)
 *   - Zero-copy communication through a shared struct
 *   - Synchronization with POSIX semaphores
 *   - Proper memory ordering concerns
 *
 * How shared memory works internally:
 *
 *   shm_open() creates a file in tmpfs (/dev/shm/).
 *   ftruncate() sets the file size (allocates pages).
 *   mmap() maps those physical pages into BOTH processes'
 *   virtual address spaces.
 *
 *   After mmap(), when process A writes to the mapped region,
 *   the CPU stores directly to the physical page.  When
 *   process B reads the same offset, it reads the same
 *   physical page — NO kernel copy involved (zero-copy).
 *
 *   BUT: you need explicit synchronization.  The CPU may
 *   reorder stores (store buffer), and caches on different
 *   cores may not be coherent at the software level without
 *   barriers.  On x86, the cache coherency protocol (MESI)
 *   handles hardware coherency, but compiler reordering
 *   still requires barriers.
 *
 * Compile: gcc -Wall -Wextra -o core_process core_process.c -lrt -lpthread
 * Run:     ./core_process
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

#define SHM_NAME    "/ipc_demo_shm"
#define SEM_EMPTY   "/ipc_shm_empty"  /* slots available for writing */
#define SEM_FULL    "/ipc_shm_full"   /* slots available for reading */

#define RING_SIZE   4
#define TOTAL_ITEMS 12

/*
 * Shared memory layout — a ring buffer.
 *
 * Both processes mmap the same region and cast it to this struct.
 * The ring buffer avoids needing a mutex for single-producer
 * single-consumer (SPSC), though we use semaphores for counting.
 *
 *   write_idx: next slot the producer will write to
 *   read_idx:  next slot the consumer will read from
 *   slots[]:   the actual data
 *
 * The semaphores implement a bounded buffer:
 *   SEM_EMPTY starts at RING_SIZE  (all slots available to write)
 *   SEM_FULL  starts at 0          (no slots available to read)
 */
struct shm_ring {
	_Atomic int write_idx;
	_Atomic int read_idx;
	int         done;       /* flag: producer is finished */
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
	int i;

	/* Cleanup from previous runs */
	shm_unlink(SHM_NAME);
	sem_unlink(SEM_EMPTY);
	sem_unlink(SEM_FULL);

	/*
	 * shm_open() — create a POSIX shared memory object.
	 *
	 * Under the hood, this creates a file in the tmpfs
	 * mounted at /dev/shm/.  It returns a file descriptor
	 * just like open().  The key difference: tmpfs pages
	 * never get written to disk — they live in RAM
	 * (and can be swapped).
	 *
	 * O_RDWR: we need to write to it after mmap.
	 */
	shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0644);
	if(shm_fd == -1) {
		perror("shm_open");
		exit(EXIT_FAILURE);
	}

	/*
	 * ftruncate() — set the size of the shared memory object.
	 *
	 * The kernel allocates pages (4KB each on x86) to back
	 * this region.  These pages are allocated lazily on first
	 * access (demand paging) unless MAP_POPULATE is used.
	 */
	if(ftruncate(shm_fd, sizeof(struct shm_ring)) == -1) {
		perror("ftruncate");
		close(shm_fd);
		shm_unlink(SHM_NAME);
		exit(EXIT_FAILURE);
	}

	/*
	 * mmap() — map the shared memory into our virtual address space.
	 *
	 * NULL:       let the kernel choose the virtual address
	 * sizeof():   length of the mapping
	 * PROT_READ|PROT_WRITE:  we can read and write
	 * MAP_SHARED: changes are visible to other processes that
	 *             map the same object.  (MAP_PRIVATE would
	 *             create a copy-on-write private mapping.)
	 * shm_fd:     the file descriptor from shm_open()
	 * 0:          offset from the start of the file
	 *
	 * The kernel creates a VMA (vm_area_struct) in our
	 * process's mm_struct.  The first access triggers a
	 * page fault — the kernel then maps the physical page
	 * into our page table.
	 *
	 * After this call, we can read/write `ring->slots[i]`
	 * as if it were local memory.  No syscalls needed for
	 * the actual data transfer — just CPU load/store.
	 */
	ring = mmap(NULL, sizeof(struct shm_ring),
	            PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if(ring == MAP_FAILED) {
		perror("mmap");
		close(shm_fd);
		shm_unlink(SHM_NAME);
		exit(EXIT_FAILURE);
	}

	/*
	 * We can close the fd after mmap — the mapping holds
	 * a reference to the underlying pages.  The pages
	 * persist until munmap() or process exit.
	 */
	close(shm_fd);

	/* Initialize shared state */
	atomic_store(&ring->write_idx, 0);
	atomic_store(&ring->read_idx, 0);
	ring->done = 0;

	/* Create semaphores for bounded buffer */
	sem_empty = sem_open(SEM_EMPTY, O_CREAT | O_EXCL, 0644, RING_SIZE);
	sem_full  = sem_open(SEM_FULL,  O_CREAT | O_EXCL, 0644, 0);
	if(sem_empty == SEM_FAILED || sem_full == SEM_FAILED) {
		perror("sem_open");
		munmap(ring, sizeof(struct shm_ring));
		shm_unlink(SHM_NAME);
		exit(EXIT_FAILURE);
	}

	printf("[producer] PID=%d, shared memory + semaphores created\n",
	       getpid());
	printf("[producer] ring buffer: %d slots, producing %d items\n",
	       RING_SIZE, TOTAL_ITEMS);

	for(i = 0; i < TOTAL_ITEMS; i++) {
		/*
		 * Wait for an empty slot.
		 *
		 * SEM_EMPTY tracks how many slots are available
		 * for writing.  If 0, we block until the consumer
		 * reads a slot and posts SEM_EMPTY.
		 */
		sem_wait(sem_empty);

		/* Write directly into shared memory — zero copy */
		int idx = atomic_load(&ring->write_idx) % RING_SIZE;
		ring->slots[idx].id = i;
		snprintf(ring->slots[idx].data,
		         sizeof(ring->slots[idx].data),
		         "Item-%d [PID %d]", i, getpid());

		/*
		 * Memory barrier: ensure the slot data is fully
		 * written before we advance write_idx.
		 *
		 * atomic_store with memory_order_release ensures
		 * all preceding stores are visible before this
		 * store becomes visible to other threads/processes
		 * that use memory_order_acquire.
		 *
		 * On x86, stores already have release semantics,
		 * but this is portable and correct on ARM/etc.
		 */
		atomic_store_explicit(&ring->write_idx,
		                      atomic_load(&ring->write_idx) + 1,
		                      memory_order_release);

		printf("[producer] wrote slot[%d]: id=%d\n", idx, i);

		/* Signal: one more slot available for reading */
		sem_post(sem_full);

		usleep(80000); /* 80ms */
	}

	/* Signal completion */
	ring->done = 1;
	sem_post(sem_full); /* wake consumer if it's waiting */

	sem_close(sem_empty);
	sem_close(sem_full);

	printf("[producer] done\n");

	/*
	 * munmap() — unmap the shared memory from our address space.
	 * The VMA is removed.  Pages remain because shm_open()'s
	 * reference (the name in /dev/shm/) still holds them.
	 *
	 * We don't shm_unlink() here — let the consumer clean up.
	 */
	munmap(ring, sizeof(struct shm_ring));

	return 0;
}
