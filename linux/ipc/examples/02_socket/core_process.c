/*
 * core_process.c — UNIX Domain Socket: Stream Server
 *
 * Demonstrates:
 *   - Creating a UNIX domain socket (AF_UNIX, SOCK_STREAM)
 *   - Binding to a filesystem path
 *   - Listening, accepting a single client
 *   - Bidirectional communication (send + recv)
 *   - Proper cleanup of the socket file
 *
 * UNIX domain sockets never touch the network stack.
 * Data moves through kernel socket buffers (sk_buff chains)
 * between two processes on the same host.
 *
 * SOCK_STREAM gives TCP-like semantics:
 *   - Connection-oriented
 *   - Reliable, ordered byte stream
 *   - No message boundaries (caller must frame messages)
 *
 * Compile: gcc -Wall -Wextra -o core_process core_process.c
 * Run:     ./core_process
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

/*
 * Socket path in the filesystem namespace.
 * The kernel creates a special inode (S_IFSOCK) at this path.
 * Path length limit: sizeof(sun_path) - 1 = 107 bytes.
 *
 * Abstract namespace alternative: sun_path[0] = '\0'
 * followed by a name.  No filesystem entry is created;
 * the socket lives purely in kernel memory.  Disappears
 * when all fds to it are closed.
 */
#define SOCKET_PATH "/tmp/ipc_unix.sock"
#define MSG_COUNT   5

/*
 * Wire protocol: fixed-size message with a type tag.
 * Both sides must agree on the layout.
 * In real systems, use length-prefixed or TLV encoding.
 */
struct ipc_msg {
	int  type;       /* 1=request, 2=response */
	int  seq;
	char data[128];
};

int
main(void)
{
	int listen_fd, client_fd;
	struct sockaddr_un addr;
	struct ipc_msg msg;
	int i;
	ssize_t n;

	/* Remove stale socket file from previous runs.
	 * bind() will fail with EADDRINUSE if the file exists. */
	unlink(SOCKET_PATH);

	/*
	 * socket(AF_UNIX, SOCK_STREAM, 0)
	 *
	 * Kernel allocates:
	 *   - struct socket     (protocol-independent wrapper)
	 *   - struct sock       (unix_sock, containing sk_buff queues)
	 *   - struct file       (so the fd can go through VFS)
	 *
	 * AF_UNIX selects the unix_family_ops which bypasses
	 * the entire TCP/IP stack.
	 */
	listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(listen_fd == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	/*
	 * bind() — associate the socket with a filesystem path.
	 *
	 * The kernel:
	 * 1. Creates a special inode at SOCKET_PATH (via mknod internally)
	 * 2. Links the inode to our struct sock
	 * 3. Other processes can find us by opening this path
	 *
	 * sun_family must be AF_UNIX.
	 * sun_path is the filesystem path (null-terminated).
	 */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if(bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind");
		close(listen_fd);
		exit(EXIT_FAILURE);
	}

	/*
	 * listen(fd, backlog)
	 *
	 * Marks the socket as passive (server).
	 * backlog=5 means the kernel will queue up to 5 pending
	 * connections.  Connection #6 gets ECONNREFUSED.
	 *
	 * Internally sets sock->state = SS_LISTENING and
	 * initializes the accept queue.
	 */
	if(listen(listen_fd, 5) == -1) {
		perror("listen");
		close(listen_fd);
		unlink(SOCKET_PATH);
		exit(EXIT_FAILURE);
	}

	printf("[server] listening on %s (PID=%d)\n",
	       SOCKET_PATH, getpid());

	/*
	 * accept() — block until a client connects.
	 *
	 * Returns a NEW fd (client_fd) representing the
	 * connected session.  The listen_fd stays open for
	 * more connections.
	 *
	 * Kernel creates a new struct sock (connected pair)
	 * and links it to the client's sock via the internal
	 * unix_peer pointer.
	 */
	client_fd = accept(listen_fd, NULL, NULL);
	if(client_fd == -1) {
		perror("accept");
		close(listen_fd);
		unlink(SOCKET_PATH);
		exit(EXIT_FAILURE);
	}

	printf("[server] client connected\n");

	/*
	 * Bidirectional exchange:
	 *   1. Receive request from client
	 *   2. Send response back
	 *
	 * recv/send on UNIX domain sockets copy data through
	 * kernel sk_buff chains.  No network headers, no
	 * checksums, no routing — just memcpy between buffers.
	 *
	 * For SOCK_STREAM:
	 *   - recv() may return fewer bytes than requested
	 *     (stream semantics — no message boundaries)
	 *   - We use fixed-size structs, so partial reads are
	 *     unlikely for small messages, but production code
	 *     must loop on recv until the full message arrives.
	 */
	for(i = 0; i < MSG_COUNT; i++) {
		/* Receive request */
		n = recv(client_fd, &msg, sizeof(msg), MSG_WAITALL);
		if(n <= 0) {
			if(n == 0)
				printf("[server] client disconnected\n");
			else
				perror("recv");
			break;
		}

		printf("[server] recv: type=%d seq=%d data=\"%s\"\n",
		       msg.type, msg.seq, msg.data);

		/* Build response */
		memset(&msg, 0, sizeof(msg));
		msg.type = 2; /* response */
		msg.seq  = i;
		snprintf(msg.data, sizeof(msg.data),
		         "ACK for request #%d", i);

		n = send(client_fd, &msg, sizeof(msg), 0);
		if(n == -1) {
			perror("send");
			break;
		}

		printf("[server] sent: seq=%d data=\"%s\"\n",
		       msg.seq, msg.data);
	}

	/*
	 * Cleanup:
	 *
	 * close(client_fd) — kernel decrements refcount on the
	 *   connected sock.  The peer gets EOF (recv returns 0).
	 *
	 * close(listen_fd) — releases the listening socket.
	 *
	 * unlink(SOCKET_PATH) — removes the filesystem entry.
	 *   Without this, the socket file lingers and the next
	 *   run's bind() fails with EADDRINUSE.
	 *
	 * If the server crashes without unlinking, the stale
	 * file persists.  Robust servers unlink before bind
	 * (as we do above) or use abstract namespace sockets.
	 */
	close(client_fd);
	close(listen_fd);
	unlink(SOCKET_PATH);

	printf("[server] done, socket cleaned up\n");

	return 0;
}
