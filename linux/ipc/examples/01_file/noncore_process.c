/*
 * noncore_process.c — File-Based IPC: Reader with Advisory Locking
 *
 * Demonstrates:
 *   - Polling for file existence (coordination)
 *   - Reading structured data written by another process
 *   - Using fcntl() advisory read (shared) locks
 *
 * Compile: gcc -Wall -Wextra -o noncore_process noncore_process.c
 * Run:     ./noncore_process  (after starting core_process)
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

struct ipc_msg {
	int   seq;
	pid_t sender;
	char  payload[64];
};

/*
 * acquire_read_lock — set a shared (read) lock on the entire file
 *
 * Multiple processes can hold F_RDLCK simultaneously.
 * But F_RDLCK blocks if another process holds F_WRLCK.
 * This prevents reading while the writer is mid-write.
 */
static int
acquire_read_lock(int fd)
{
	struct flock fl;

	memset(&fl, 0, sizeof(fl));
	fl.l_type   = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start  = 0;
	fl.l_len    = 0;

	if(fcntl(fd, F_SETLKW, &fl) == -1) {
		perror("fcntl F_RDLCK");
		return -1;
	}
	return 0;
}

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

/*
 * wait_for_ready — poll for the existence of READY_FILE
 *
 * access(path, F_OK) returns 0 if the file exists.
 * It checks the inode via the dentry cache — fast path
 * if the dentry is cached, slow path triggers a lookup
 * through the filesystem.
 *
 * Polling like this wastes CPU.  In production, use
 * inotify(7) to get kernel notifications when the file
 * is created.  But polling is explicit and easy to debug.
 */
static void
wait_for_ready(void)
{
	int waited = 0;

	printf("[noncore] waiting for ready file...\n");
	while(access(READY_FILE, F_OK) == -1) {
		usleep(100000); /* 100ms */
		waited++;
		if(waited % 50 == 0)
			printf("[noncore] still waiting (%d seconds)...\n",
			       waited / 10);
	}
	printf("[noncore] ready file found\n");
}

int
main(void)
{
	int fd;
	struct ipc_msg msg;
	ssize_t n;
	int count;

	wait_for_ready();

	/*
	 * O_RDONLY — open for reading only.
	 * The kernel creates a new struct file with f_mode
	 * set to FMODE_READ.  f_pos starts at 0.
	 */
	fd = open(DATA_FILE, O_RDONLY);
	if(fd == -1) {
		perror("open data file");
		exit(EXIT_FAILURE);
	}

	printf("[noncore] PID=%d reading from %s\n", getpid(), DATA_FILE);

	count = 0;
	for(;;) {
		if(acquire_read_lock(fd) == -1) {
			close(fd);
			exit(EXIT_FAILURE);
		}

		/*
		 * read() copies bytes from the page cache into our
		 * userspace buffer.  Returns:
		 *   > 0  — number of bytes read
		 *   0    — EOF (f_pos reached i_size)
		 *   -1   — error
		 *
		 * If the writer is mid-write and we have no lock,
		 * we might see a partial struct — a torn read.
		 * The lock prevents this.
		 */
		n = read(fd, &msg, sizeof(msg));

		if(release_lock(fd) == -1) {
			close(fd);
			exit(EXIT_FAILURE);
		}

		if(n == 0) {
			/* EOF — no more data */
			break;
		}

		if(n != sizeof(msg)) {
			fprintf(stderr,
			        "[noncore] partial read: %zd bytes "
			        "(expected %zu)\n",
			        n, sizeof(msg));
			break;
		}

		printf("[noncore] read seq=%d sender=%d payload=\"%s\"\n",
		       msg.seq, msg.sender, msg.payload);
		count++;
	}

	close(fd);

	printf("[noncore] total messages read: %d\n", count);

	/* Cleanup: remove the files we used */
	unlink(DATA_FILE);
	unlink(READY_FILE);
	printf("[noncore] cleaned up temp files\n");

	return 0;
}
