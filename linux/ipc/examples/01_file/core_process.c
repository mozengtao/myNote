/*
 * core_process.c — File-Based IPC: Writer with Advisory Locking
 *
 * Demonstrates:
 *   - Writing structured data to a regular file
 *   - Using fcntl() advisory write locks to prevent races
 *   - Coordinating with a reader process via a ready-flag file
 *
 * The kernel does NOT enforce advisory locks automatically.
 * Both processes must cooperate by requesting locks.
 * If a process ignores locking, it can still read/write freely.
 *
 * Compile: gcc -Wall -Wextra -o core_process core_process.c
 * Run:     ./core_process
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define DATA_FILE   "/tmp/ipc_data.bin"
#define READY_FILE  "/tmp/ipc_ready"
#define MSG_COUNT   5

/* Structured message — both processes share this layout */
struct ipc_msg {
	int   seq;
	pid_t sender;
	char  payload[64];
};

/*
 * acquire_write_lock — set an exclusive (write) lock on the entire file
 *
 * Uses fcntl(F_SETLKW) which blocks until the lock is available.
 *
 * struct flock fields:
 *   l_type   = F_WRLCK   exclusive lock (readers and writers blocked)
 *   l_whence = SEEK_SET   lock offset relative to start of file
 *   l_start  = 0          lock starts at byte 0
 *   l_len    = 0          0 means "lock entire file to EOF"
 *
 * fcntl() locks are per-process, per-inode.  If any fd in THIS process
 * to the same inode is closed, ALL locks on that inode are released.
 * This is a notorious footgun.
 */
static int
acquire_write_lock(int fd)
{
	struct flock fl;

	memset(&fl, 0, sizeof(fl));
	fl.l_type   = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start  = 0;
	fl.l_len    = 0; /* entire file */

	if(fcntl(fd, F_SETLKW, &fl) == -1) {
		perror("fcntl F_SETLKW");
		return -1;
	}
	return 0;
}

/*
 * release_lock — release the lock on the entire file
 */
static int
release_lock(int fd)
{
	struct flock fl;

	memset(&fl, 0, sizeof(fl));
	fl.l_type   = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start  = 0;
	fl.l_len    = 0;

	if(fcntl(fd, F_SETLK, &fl) == -1) {
		perror("fcntl F_UNLCK");
		return -1;
	}
	return 0;
}

int
main(void)
{
	int fd;
	struct ipc_msg msg;
	int i;

	/* Remove stale files from previous runs */
	unlink(DATA_FILE);
	unlink(READY_FILE);

	/*
	 * O_CREAT | O_RDWR — create if absent, open for read+write.
	 * O_TRUNC — truncate to zero bytes if file already exists.
	 * 0644 — owner rw, group r, others r.
	 *
	 * The fd is our handle to the inode.  The kernel's struct file
	 * tracks the current offset (f_pos), open mode, and a pointer
	 * to the inode (which holds the actual data blocks on disk or
	 * in page cache).
	 */
	fd = open(DATA_FILE, O_CREAT | O_RDWR | O_TRUNC, 0644);
	if(fd == -1) {
		perror("open data file");
		exit(EXIT_FAILURE);
	}

	printf("[core] PID=%d writing %d messages to %s\n",
	       getpid(), MSG_COUNT, DATA_FILE);

	for(i = 0; i < MSG_COUNT; i++) {
		/*
		 * Acquire exclusive lock BEFORE writing.
		 *
		 * Internally, the kernel checks a linked list of
		 * struct file_lock hanging off the inode.  If a
		 * conflicting lock exists (shared OR exclusive held
		 * by another process), the caller sleeps in a wait
		 * queue until that lock is released.
		 */
		if(acquire_write_lock(fd) == -1) {
			close(fd);
			exit(EXIT_FAILURE);
		}

		/* Build the message */
		memset(&msg, 0, sizeof(msg));
		msg.seq    = i;
		msg.sender = getpid();
		snprintf(msg.payload, sizeof(msg.payload),
		         "Message #%d from core", i);

		/*
		 * write() copies bytes from userspace (msg) into the
		 * kernel page cache.  The actual disk write happens
		 * later (writeback), unless we call fsync().
		 *
		 * The fd's f_pos advances by sizeof(msg) bytes.
		 */
		if(write(fd, &msg, sizeof(msg)) != sizeof(msg)) {
			perror("write");
			release_lock(fd);
			close(fd);
			exit(EXIT_FAILURE);
		}

		/*
		 * fsync() forces the kernel to flush dirty pages in
		 * the page cache to the underlying block device and
		 * waits for the device to confirm the write.
		 * This ensures the reader sees our data even if the
		 * system crashes.
		 */
		if(fsync(fd) == -1) {
			perror("fsync");
		}

		printf("[core] wrote seq=%d\n", msg.seq);

		if(release_lock(fd) == -1) {
			close(fd);
			exit(EXIT_FAILURE);
		}

		/* Simulate work between writes */
		usleep(200000); /* 200ms */
	}

	/*
	 * Create a flag file to signal the reader that all
	 * data has been written.  This is a simple coordination
	 * mechanism — the reader polls for this file's existence.
	 *
	 * Better alternatives: FIFO, eventfd, inotify.
	 * But for teaching file-based IPC, this is clear.
	 */
	fd = open(READY_FILE, O_CREAT | O_WRONLY, 0644);
	if(fd == -1) {
		perror("open ready file");
		exit(EXIT_FAILURE);
	}
	close(fd);

	printf("[core] all messages written, ready file created\n");
	printf("[core] done\n");

	return 0;
}
