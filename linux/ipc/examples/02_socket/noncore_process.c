/*
 * noncore_process.c — UNIX Domain Socket: Stream Client
 *
 * Demonstrates:
 *   - Connecting to a UNIX domain stream socket
 *   - Sending requests and receiving responses
 *   - Handling connection lifecycle
 *
 * Compile: gcc -Wall -Wextra -o noncore_process noncore_process.c
 * Run:     ./noncore_process  (after starting core_process)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/ipc_unix.sock"
#define MSG_COUNT   5

struct ipc_msg {
	int  type;
	int  seq;
	char data[128];
};

int
main(void)
{
	int sock_fd;
	struct sockaddr_un addr;
	struct ipc_msg msg;
	int i;
	ssize_t n;

	/*
	 * Client socket — same domain and type as server.
	 * No bind() needed; the kernel auto-assigns an
	 * anonymous address.
	 */
	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(sock_fd == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

	/*
	 * connect() — initiate connection to the server.
	 *
	 * Kernel looks up the inode at SOCKET_PATH, verifies
	 * it's a socket inode, and links our sock to the
	 * server's sock via unix_peer.
	 *
	 * If the server isn't listening yet, connect() fails
	 * with ECONNREFUSED or ENOENT.  A robust client would
	 * retry with backoff.
	 */
	printf("[client] connecting to %s (PID=%d)\n",
	       SOCKET_PATH, getpid());

	if(connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("connect");
		close(sock_fd);
		exit(EXIT_FAILURE);
	}

	printf("[client] connected to server\n");

	for(i = 0; i < MSG_COUNT; i++) {
		/* Build request */
		memset(&msg, 0, sizeof(msg));
		msg.type = 1; /* request */
		msg.seq  = i;
		snprintf(msg.data, sizeof(msg.data),
		         "Request #%d from client", i);

		/*
		 * send() copies our message into the kernel's
		 * sk_buff queue for the connected peer.
		 *
		 * If the peer's receive buffer is full, send()
		 * blocks (for blocking sockets) until space opens.
		 *
		 * If the peer has closed their fd, send() gets
		 * SIGPIPE (default: kill the process) or returns
		 * EPIPE if MSG_NOSIGNAL is set.
		 */
		n = send(sock_fd, &msg, sizeof(msg), 0);
		if(n == -1) {
			perror("send");
			break;
		}

		printf("[client] sent: seq=%d data=\"%s\"\n",
		       msg.seq, msg.data);

		/*
		 * recv() with MSG_WAITALL:
		 *
		 * Tells the kernel to block until exactly
		 * sizeof(msg) bytes have been received, rather
		 * than returning early with a partial read.
		 *
		 * Without MSG_WAITALL on a stream socket, recv()
		 * can return as soon as ANY data is available,
		 * even 1 byte.
		 */
		n = recv(sock_fd, &msg, sizeof(msg), MSG_WAITALL);
		if(n <= 0) {
			if(n == 0)
				printf("[client] server disconnected\n");
			else
				perror("recv");
			break;
		}

		printf("[client] recv: type=%d seq=%d data=\"%s\"\n",
		       msg.type, msg.seq, msg.data);
	}

	/*
	 * Closing the client fd triggers:
	 *   1. Kernel sends EOF to the peer (recv returns 0)
	 *   2. Decrements refcount on our sock struct
	 *   3. When refcount hits 0, sk_buff queue is freed
	 *
	 * No need to unlink — client doesn't own the socket path.
	 */
	close(sock_fd);

	printf("[client] done\n");

	return 0;
}
