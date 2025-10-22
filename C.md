[C Online Compiler](https://www.programiz.com/c-programming/online-compiler/)  
[va_list in C: exploring ft_printf](https://medium.com/@turman1701/va-list-in-c-exploring-ft-printf-bb2a19fcd128)  
[SystemProgramming Wiki](https://csresources.github.io/SystemProgrammingWiki/SystemProgramming/C-Programming,-Part-1:-Introduction/)  
[]()  
[]()  
[]()  
[]()  

[Understanding the C Runtime: crt0, crt1, crti, and crtn](https://www.inferara.com/en/blog/c-runtime/)  
[C/C++ Runtime Startup](https://etherealwake.com/2021/09/crt-startup/)  
[Explanation of crt0, crt1, crt1, and crtn (C- Runtime)](https://thejat.in/learn/explanation-of-crt0-crt1-crt1-and-crtn-c-runtime)  
[CRT: C Run Time Before Starting main()](https://vishalchovatiya.com/posts/crt-run-time-before-starting-main/)  
[**Linux-dlsym**](https://lambertxiao.github.io/posts/linux-dlsym/doc/)  
[真正理解 RTLD_NEXT 的作用](https://csstormq.github.io/)  
[Dynamic Linking Example](https://vishalchovatiya.com/posts/dynamic-linking-example/)  
[]()  
[]()  
[]()  
[How to make a system call in C](https://jameshfisher.com/2018/02/19/how-to-syscall-in-c/)  
[How do I call a program from C?](https://jameshfisher.com/2017/02/07/how-do-i-call-a-program-in-c/)  
[How do I call a program in C, setting up standard pipes?](https://jameshfisher.com/2017/02/17/how-do-i-call-a-program-in-c-with-pipes/)  
[How do I use fork in C?](https://jameshfisher.com/2017/02/06/how-do-i-use-fork-in-c/)  
[What is a “file descriptor”, really?](https://jameshfisher.com/2016/12/15/file-descriptor-misnomer/)  
[What does getaddrinfo do?](https://jameshfisher.com/2018/02/03/what-does-getaddrinfo-do/)  
[What is a a FIFO, or “named pipe”? What is mkfifo in C?](https://jameshfisher.com/2017/02/21/mkfifo-c/)  
[What is static linking in C?](https://jameshfisher.com/2017/08/26/static-linking/)  
[What are setjmp and longjmp in C?](https://jameshfisher.com/2017/02/18/what-are-setjmp-longjmp/)  
[How to generate Intel and AT&T assembly with clang](https://jameshfisher.com/2017/02/19/clang-assembly-att-intel/)  
[How do I duplicate a file descriptor in C?](https://jameshfisher.com/2017/02/15/c-dup-syscall/)  
[How do I use execve in C?](https://jameshfisher.com/2017/02/05/how-do-i-use-execve-in-c/)  
[How do I generate assembly from a C file?](https://jameshfisher.com/2017/02/03/c-generate-assembly/)  
[How can I read a file with mmap in C?](https://jameshfisher.com/2017/01/27/mmap-file-read/)  
[What is realloc in C?](https://jameshfisher.com/2016/12/01/c-realloc/)  
[What is mmap in C?](https://jameshfisher.com/2017/01/26/mmap/)  
[How can I write a file with mmap in C?](https://jameshfisher.com/2017/01/28/mmap-file-write/)  
[In what ways can processes communicate?](https://jameshfisher.com/2017/01/29/process-communication-methods/)  
[What are ‘signals’ in C?](https://jameshfisher.com/2017/01/09/c-signals/)  
[What does the C signal function return?](https://jameshfisher.com/2017/01/10/c-signal-return-value/)  
[How do I unregister a signal handler in C?](https://jameshfisher.com/2017/01/11/c-signal-unregister/)  
[What does void mean as a function parameter in C?](https://jameshfisher.com/2016/11/27/c-void-params/)  
[What are lvalue and rvalue in C?](https://jameshfisher.com/2017/01/21/c-lvalue-rvalue/)  
[What are the domain and type arguments to the socket system call?](https://jameshfisher.com/2017/02/27/socket-types/)  
[Variadic Function Working in C](https://www.thejat.in/blog/variadic-function-working-in-c)  
[Understanding Complex C/C++ Declarations](https://www.thejat.in/blog/understanding-complex-cc-declarations)  
[Exploring Singleton Pattern in C++: Ensuring Unique Instances](https://www.thejat.in/blog/exploring-singleton-pattern-in-c-ensuring-unique-instances)  
[]()  

## 简化版插件架构示例
- 文件结构
```
plugin
├── main.c            # 主程序
├── plugin.h           # 插件接口定义
├── plugin_a.c        # 插件A实现
├── plugin_b.c        # 插件B实现
└── Makefile          # 编译脚本
```
- plugin.h, plugin_a.c, plugin_b.c
```c
// plugin.h
#ifndef PLUGIN_H
#define PLUGIN_H

// 插件接口结构体
typedef struct
{
	const char *name;
	void (*init)(void);
	void (*execute)(void);
	void (*cleanup)(void);
} Plugin;

// 插件注册函数类型
typedef Plugin *(*RegisterPluginFunc)(void);

#endif // PLUGIN_H

// plugin_a.c
#include <stdio.h>
#include "plugin.h"

// 插件A的初始化函数
static void plugin_a_init(void)
{
	printf("Plugin A initialized\n");
}

// 插件A的执行函数
static void plugin_a_execute(void)
{
	printf("Plugin A executing...\n");
}

// 插件A的清理函数
static void plugin_a_cleanup(void)
{
	printf("Plugin A cleaned up\n");
}

// 插件A的接口实例
static Plugin plugin_a = {
	.name = "PluginA",
	.init = plugin_a_init,
	.execute = plugin_a_execute,
	.cleanup = plugin_a_cleanup};

// 插件注册函数
__attribute__((visibility("default")))
Plugin *
register_plugin(void)
{
	return &plugin_a;
}

// plugin_b.c
#include <stdio.h>
#include "plugin.h"

// 插件B的初始化函数
static void plugin_b_init(void)
{
	printf("Plugin B initialized\n");
}

// 插件B的执行函数
static void plugin_b_execute(void)
{
	printf("Plugin B executing...\n");
}

// 插件B的清理函数
static void plugin_b_cleanup(void)
{
	printf("Plugin B cleaned up\n");
}

// 插件B的接口实例
static Plugin plugin_b = {
	.name = "PluginB",
	.init = plugin_b_init,
	.execute = plugin_b_execute,
	.cleanup = plugin_b_cleanup};

// 插件注册函数
__attribute__((visibility("default")))
Plugin *
register_plugin(void)
{
	return &plugin_b;
}

// main.c
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include "plugin.h"

#define PLUGIN_DIR "./plugins"

int main()
{
	DIR *dir;
	struct dirent *entry;

	// 打开插件目录
	if ((dir = opendir(PLUGIN_DIR)) == NULL)
	{
		perror("opendir");
		return EXIT_FAILURE;
	}

	// 遍历插件目录
	while ((entry = readdir(dir)) != NULL)
	{
		// 只处理.so文件
		if (entry->d_type != DT_REG)
			continue;
		char *ext = strrchr(entry->d_name, '.');
		if (!ext || strcmp(ext, ".so") != 0)
			continue;

		// 构建完整路径
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", PLUGIN_DIR, entry->d_name);

		// 加载插件
		void *handle = dlopen(path, RTLD_LAZY);
		if (!handle)
		{
			fprintf(stderr, "Error loading %s: %s\n", path, dlerror());
			continue;
		}

		// 获取注册函数
		RegisterPluginFunc register_plugin = dlsym(handle, "register_plugin");
		if (!register_plugin)
		{
			fprintf(stderr, "Error finding register_plugin in %s: %s\n", path, dlerror());
			dlclose(handle);
			continue;
		}

		// 注册并获取插件接口
		Plugin *plugin = register_plugin();
		if (!plugin)
		{
			fprintf(stderr, "Plugin registration failed for %s\n", path);
			dlclose(handle);
			continue;
		}

		// 使用插件
		printf("\n=== Using plugin: %s ===\n", plugin->name);
		plugin->init();
		plugin->execute();
		plugin->cleanup();

		// 关闭插件
		dlclose(handle);
	}

	closedir(dir);
	return EXIT_SUCCESS;
}
```

- Makefile
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -fPIC
LDFLAGS = -ldl

# 主程序目标
TARGET = plugin_demo

# 插件目录
PLUGIN_DIR = plugins

# 插件目标
PLUGINS = $(PLUGIN_DIR)/plugin_a.so $(PLUGIN_DIR)/plugin_b.so

all: $(TARGET) $(PLUGINS)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(PLUGIN_DIR)/plugin_a.so: plugin_a.c plugin.h
	@mkdir -p $(PLUGIN_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $<

$(PLUGIN_DIR)/plugin_b.so: plugin_b.c plugin.h
	@mkdir -p $(PLUGIN_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(PLUGINS)
	rmdir $(PLUGIN_DIR) 2>/dev/null || true

.PHONY: all run clean

```

## string
[Library Functions](https://www.ibm.com/docs/en/i/7.4.0?topic=functions-library)  
[String and Array Utilities](https://www.gnu.org/software/libc/manual/html_node/String-and-Array-Utilities.html)  
[The String Span Functions](https://c-for-dummies.com/blog/?p=5068)  
[Parsing Words with the strspn() Function](https://c-for-dummies.com/blog/?p=5072)  
[Slicing Strings with strsep()](https://c-for-dummies.com/blog/?p=1769)  
[How to Use strtok and strtok_r in C](https://systems-encyclopedia.cs.illinois.edu/articles/c-strtok/)  
[strtok](https://icarus.cs.weber.edu/~dab/cs1410/textbook/8.Strings/more_cstring.html)  
[The Standard C Library for Linux, part 7: String Handling](https://linuxgazette.net/issue76/rogers.html)  
[C Programming/String manipulation](https://en.wikibooks.org/wiki/C_Programming/String_manipulation)  
[String Functions in C (Full List With Examples)](https://www.wscubetech.com/resources/c-programming/string-functions)  
[Standard C Library Functions Table, By Name](https://www.ibm.com/docs/en/i/7.5.0?topic=extensions-standard-c-library-functions-table-by-name)  
[]()  
[]()  

## socket
```c
//
#include <sys/types.h>
#include <sys/socket.h>

/*
 * domain: specifies an address family.
 * type: specifies communication semantics.
 * protocol: specifies a concrete protocol type
*/
int socket(int domain, int type, int protocol);

// domain
/*
| Macro                  | Description                     | Struct Used               | Typical Use                       |
| ---------------------- | ------------------------------- | ------------------------- | --------------------------------- |
| `AF_INET`              | IPv4 Internet protocols         | `struct sockaddr_in`      | IPv4 TCP/UDP communication        |
| `AF_INET6`             | IPv6 Internet protocols         | `struct sockaddr_in6`     | IPv6 TCP/UDP communication        |
| `AF_UNIX` / `AF_LOCAL` | Local communication (same host) | `struct sockaddr_un`      | UNIX domain sockets               |
| `AF_PACKET`            | Raw packet access (Layer 2)     | `struct sockaddr_ll`      | Network sniffers, raw Ethernet    |
| `AF_NETLINK`           | Kernel-user communication       | `struct sockaddr_nl`      | Routing, Netfilter, kernel events |
| `AF_BLUETOOTH`         | Bluetooth protocol              | `struct sockaddr_rc` etc. | Bluetooth communication           |

// type
| Macro            | Description                          | Semantics                          |
| ---------------- | ------------------------------------ | ---------------------------------- |
| `SOCK_STREAM`    | Stream-oriented (connection-based)   | Reliable byte stream (TCP)         |
| `SOCK_DGRAM`     | Datagram-oriented (connectionless)   | Unreliable message (UDP)           |
| `SOCK_RAW`       | Raw network protocol access          | Direct access to IP layer          |
| `SOCK_SEQPACKET` | Sequenced, reliable, record-oriented | Used with UNIX domain or Bluetooth |
| `SOCK_PACKET`    | (Deprecated) raw packets             | Legacy code only                   |

// protocol
| Domain      | Type          | Protocol           | Meaning                     |
| ----------- | ------------- | ------------------ | --------------------------- |
| `AF_INET`   | `SOCK_STREAM` | `IPPROTO_TCP`      | TCP                         |
| `AF_INET`   | `SOCK_DGRAM`  | `IPPROTO_UDP`      | UDP                         |
| `AF_INET`   | `SOCK_RAW`    | `IPPROTO_ICMP`     | Raw ICMP socket             |
| `AF_PACKET` | `SOCK_RAW`    | `htons(ETH_P_ALL)` | Capture all Ethernet frames |
| `AF_UNIX`   | `SOCK_STREAM` | `0`                | Local stream socket         |
*/

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
}



// ipv4
struct sockaddr_in {
    sa_family_t    sin_family; // Address family (AF_INET)
    in_port_t      sin_port;   // Port number (network byte order)
    struct in_addr sin_addr;   // IP address
    unsigned char  sin_zero[8]; // Padding
};

struct in_addr {
  uint32_t       s_addr;     /* address in network byte order */
};

// ipv6
struct sockaddr_in6 {
    sa_family_t     sin6_family;   // AF_INET6
    in_port_t       sin6_port;     // Port number
    uint32_t        sin6_flowinfo; // Flow information
    struct in6_addr sin6_addr;     // IPv6 address
    uint32_t        sin6_scope_id; // Scope ID
};

struct in6_addr {
  unsigned char   s6_addr[16];   /* IPv6 address */
};

// unix domain socket
struct sockaddr_un {
    sa_family_t sun_family;              // AF_UNIX
    char        sun_path[108];           // Pathname
};

// tcp
// server.c
// tcp_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[1024];

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind to a port
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // 3. Listen
    listen(server_fd, 5);
    printf("Server listening on port 8080...\n");

    // 4. Accept
    client_fd = accept(server_fd, NULL, NULL);
    printf("Client connected!\n");

    // 5. Communicate
    recv(client_fd, buffer, sizeof(buffer), 0);
    printf("Received: %s\n", buffer);
    send(client_fd, "Hello Client!", 13, 0);

    close(client_fd);
    close(server_fd);
    return 0;
}

// client.c
// tcp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock;
    struct sockaddr_in server;
    char buffer[1024];

    // 1. Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Set up server address
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    // 3. Connect
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // 4. Communicate
    send(sock, "Hello Server!", 13, 0);
    recv(sock, buffer, sizeof(buffer), 0);
    printf("Received: %s\n", buffer);

    close(sock);
    return 0;
}

// udp
// server.c
// udp_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[1024];
    socklen_t addr_len = sizeof(client_addr);

    // 1. Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind to a port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on port 9000...\n");

    // 3. Receive datagram
    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr*)&client_addr, &addr_len);
    buffer[n] = '\0';
    printf("Received from client: %s\n", buffer);

    // 4. Send response
    sendto(sockfd, "Hello from UDP server!", 23, 0,
           (struct sockaddr*)&client_addr, addr_len);

    close(sockfd);
    return 0;
}

// client.c
// udp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[1024];
    socklen_t addr_len = sizeof(server_addr);

    // 1. Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Define server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 3. Send datagram
    sendto(sockfd, "Hello UDP Server!", 18, 0,
           (struct sockaddr*)&server_addr, addr_len);

    // 4. Receive response
    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr*)&server_addr, &addr_len);
    buffer[n] = '\0';
    printf("Server reply: %s\n", buffer);

    close(sockfd);
    return 0;
}

// unix domain socket
// server.c
// unix_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/demo_socket"

int main() {
    int server_fd, client_fd;
    struct sockaddr_un addr;
    char buffer[100];

    unlink(SOCKET_PATH); // Remove old socket file

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 5);
    printf("UNIX server listening on %s...\n", SOCKET_PATH);

    client_fd = accept(server_fd, NULL, NULL);
    read(client_fd, buffer, sizeof(buffer));
    printf("Received: %s\n", buffer);
    write(client_fd, "Hi UNIX client!", 15);

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}

// client.c
// unix_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/demo_socket"

int main() {
    int sockfd;
    struct sockaddr_un addr;
    char buffer[100];

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    write(sockfd, "Hello UNIX server!", 19);
    read(sockfd, buffer, sizeof(buffer));
    printf("Server says: %s\n", buffer);

    close(sockfd);
    return 0;
}

// raw socket (requires root privileges)
/*
 * Raw sockets allow user-space programs to:
 * Capture or send raw Ethernet frames,
 * Bypass the TCP/UDP/IP stack,
 * Inspect headers for tools like tcpdump, Wireshark, or custom packet sniffers.
*/
// packet_sniffer.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>	// For ETH_P_ALL
#include <net/if.h>			// For if_nametoindex()

int main() {
    int sockfd;
    struct sockaddr_ll sll;
    unsigned char buffer[2048];

    // 1. Create a raw socket
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind to a specific network interface (e.g., eth0)
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex("eth0"); // replace with your NIC name
    if (sll.sll_ifindex == 0) {
        perror("if_nametoindex");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Listening on interface eth0 for raw Ethernet frames...\n");

    // 3. Receive packets
    while (1) {
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (n < 0) {
            perror("recvfrom");
            break;
        }

        struct ethhdr *eth = (struct ethhdr *)buffer;

        printf("\n--- Ethernet Frame ---\n");
        printf("Destination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->h_dest[0], eth->h_dest[1], eth->h_dest[2],
               eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
        printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->h_source[0], eth->h_source[1], eth->h_source[2],
               eth->h_source[3], eth->h_source[4], eth->h_source[5]);
        printf("EtherType: 0x%04x\n", ntohs(eth->h_proto));
        printf("Payload length: %zd bytes\n", n - sizeof(struct ethhdr));
    }

    close(sockfd);
    return 0;
}

// packet_sender.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>

int main() {
    int sockfd;
    struct sockaddr_ll sll;
    unsigned char frame[1500];

    // 1. Create raw socket
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Get interface index
    int ifindex = if_nametoindex("eth0"); // replace with your interface
    if (ifindex == 0) {
        perror("if_nametoindex");
        exit(EXIT_FAILURE);
    }

    // 3. Prepare destination
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex;
    sll.sll_halen = ETH_ALEN;
    sll.sll_addr[0] = 0xff; // Broadcast address
    sll.sll_addr[1] = 0xff;
    sll.sll_addr[2] = 0xff;
    sll.sll_addr[3] = 0xff;
    sll.sll_addr[4] = 0xff;
    sll.sll_addr[5] = 0xff;

    // 4. Build Ethernet frame
    struct ethhdr *eth = (struct ethhdr *)frame;
    memset(eth->h_dest, 0xff, ETH_ALEN);       // Destination: broadcast
    memset(eth->h_source, 0x11, ETH_ALEN);     // Fake source
    eth->h_proto = htons(0x88B5);              // Custom Ethertype
    strcpy((char *)(frame + sizeof(struct ethhdr)), "Hello Raw World!");

    // 5. Send
    ssize_t frame_len = sizeof(struct ethhdr) + strlen("Hello Raw World!");
    if (sendto(sockfd, frame, frame_len, 0, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("sendto");
    } else {
        printf("Sent raw Ethernet frame (%zd bytes)\n", frame_len);
    }

    close(sockfd);
    return 0;
}

// socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)
// icmp_ping.c
// headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>

// ICMP Header (as defined in <netinet/ip_icmp.h>)
struct icmphdr {
    uint8_t  type;      // e.g., ICMP_ECHO
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
};

// ICMP checksum function
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char*)buf;

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// ping_raw.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: sudo %s <destination IP>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in dest;
    char packet[64];
    struct icmphdr *icmp = (struct icmphdr*)packet;

    // 1. Create raw socket for ICMP
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Setup destination address
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &dest.sin_addr);

    // 3. Build ICMP Echo Request
    memset(packet, 0, sizeof(packet));
    icmp->type = ICMP_ECHO;   // 8
    icmp->code = 0;
    icmp->un.echo.id = getpid() & 0xFFFF;
    icmp->un.echo.sequence = 1;
    icmp->checksum = 0;
    icmp->checksum = checksum(packet, sizeof(packet));

    // 4. Record time and send
    struct timeval start, end;
    gettimeofday(&start, NULL);

    if (sendto(sockfd, packet, sizeof(packet), 0,
               (struct sockaddr*)&dest, sizeof(dest)) <= 0) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    // 5. Wait for reply
    char recvbuf[1024];
    struct sockaddr_in reply_addr;
    socklen_t addr_len = sizeof(reply_addr);

    ssize_t n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0,
                         (struct sockaddr*)&reply_addr, &addr_len);
    if (n < 0) {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&end, NULL);

    double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                 (end.tv_usec - start.tv_usec) / 1000.0;

    printf("Reply from %s: bytes=%zd time=%.3f ms\n",
           argv[1], n, rtt);

    close(sockfd);
    return 0;
}

// AF_UNIX (local IPC)
// server.c
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(void) {
	int fd;
	struct sockaddr_un addr;
	const char *path = "/tmp/example.sock";
	char buf[256];
	ssize_t n;
	struct sockaddr_un peer;
	socklen_t peerlen = sizeof(peer);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) return 1;

	unlink(path); // ensure clean
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

	n = recvfrom(fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&peer, &peerlen);
	if (n > 0) {
		buf[n] = '\0';
		printf("got: %s\n", buf);
	}
	close(fd);
	unlink(path);
	return 0;
}

// client.c
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	int fd;
	struct sockaddr_un dst;
	const char *path = "/tmp/example.sock";
	const char *msg = "hello";

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) return 1;

	memset(&dst, 0, sizeof(dst));
	dst.sun_family = AF_UNIX;
	strncpy(dst.sun_path, path, sizeof(dst.sun_path) - 1);

	sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&dst, sizeof(dst));
	close(fd);
	return 0;
}

// AF_INET (TCP)
// server.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(void) {
	int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr = {0};
	int c;
	if (s < 0) return 1;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(8080);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	if (listen(s, 16) < 0) return 1;

	c = accept(s, NULL, NULL);
	if (c >= 0) {
		const char *resp = "hi\n";
		send(c, resp, 3, 0);
		close(c);
	}
	close(s);
	return 0;
}

// client.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int main(void) {
	int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr = {0};
	if (s < 0) return 1;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	send(s, "hello", 5, 0);
	close(s);
	return 0;
}

// AF_INET6 (IPv6 TCP)
// server.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

int main(void) {
	int s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in6 addr = {0};
	int v6only = 1;

	if (s < 0) return 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

	addr.sin6_family = AF_INET6;
	addr.sin6_addr = in6addr_any;
	addr.sin6_port = htons(8080);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	if (listen(s, 16) < 0) return 1;

	int c = accept(s, NULL, NULL);
	if (c >= 0) close(c);
	close(s);
	return 0;
}

// client.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(void) {
	int s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in6 addr = {0};
	if (s < 0) return 1;

	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(8080);
	inet_pton(AF_INET6, "::1", &addr.sin6_addr);

	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	close(s);
	return 0;
}

// AF_NETLINK  (kernel-user communication)
// receiver.c
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(void) {
	int s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	struct sockaddr_nl addr = {0};
	char buf[8192];
	struct nlmsghdr *nlh;

	if (s < 0) return 1;

	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();      // unique user-space PID
	addr.nl_groups = RTMGRP_LINK; // subscribe to link events

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

	ssize_t n = recv(s, buf, sizeof(buf), 0);
	if (n > 0) {
		nlh = (struct nlmsghdr *)buf;
		printf("got netlink msg type=%d len=%d\n", nlh->nlmsg_type, nlh->nlmsg_len);
	}
	close(s);
	return 0;
}

// sender.c
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#include <string.h>

int main(void) {
	int s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	struct sockaddr_nl dst = {0};
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg gen;
	} req = {0};

	if (s < 0) return 1;

	dst.nl_family = AF_NETLINK;

	req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
	req.nlh.nlmsg_type = RTM_GETLINK;
	req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.gen.rtgen_family = AF_UNSPEC;

	struct iovec iov = { &req, req.nlh.nlmsg_len };
	struct msghdr msg = {0};
	msg.msg_name = &dst;
	msg.msg_namelen = sizeof(dst);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	if (sendmsg(s, &msg, 0) < 0) return 1;
	close(s);
	return 0;
}

// AF_PACKET (link layer, raw Ethernet; requires root)
// receiver.c
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main(void) {
	int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	struct sockaddr_ll addr = {0};
	char buf[2048];
	ssize_t n;

	if (s < 0) return 1;

	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ALL);
	addr.sll_ifindex = if_nametoindex("eth0");

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

	n = recv(s, buf, sizeof(buf), 0);
	if (n > 0) printf("got %zd bytes\n", n);

	close(s);
	return 0;
}

// sender.c
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
	struct sockaddr_ll dst = {0};
	unsigned char frame[64] = {0}; // fill with valid Ethernet frame

	if (s < 0) return 1;

	dst.sll_family = AF_PACKET;
	dst.sll_ifindex = if_nametoindex("eth0");
	dst.sll_halen = ETH_ALEN;
	// set dst.sll_addr[0..5] to target MAC

	sendto(s, frame, sizeof(frame), 0, (struct sockaddr *)&dst, sizeof(dst));
	close(s);
	return 0;
}

// function prototype for setsockopt
int setsockopt(int sockfd, int level, int optname,
               const void *optval, socklen_t optlen);

// SOL_SOCKET: socket level options
| Option                                |       Value type | Purpose / typical use                                                             |
| ------------------------------------- | ---------------: | --------------------------------------------------------------------------------- |
| `SO_REUSEADDR`                        |      `int` (0/1) | Allow binding to an address in `TIME_WAIT`. Common on servers to restart quickly. |
| `SO_REUSEPORT`                        |      `int` (0/1) | Allow multiple sockets bind same (addr,port) (load-sharing on some kernels).      |
| `SO_KEEPALIVE`                        |      `int` (0/1) | Enable TCP keepalive probes (basic on/off; details set via TCP_* options).        |
| `SO_BROADCAST`                        |      `int` (0/1) | Enable sending datagrams to broadcast addresses (UDP).                            |
| `SO_RCVBUF` / `SO_SNDBUF`             |    `int` (bytes) | Set kernel receive / send buffer sizes.                                           |
| `SO_RCVTIMEO` / `SO_SNDTIMEO`         | `struct timeval` | Set blocking I/O timeouts for `recv`/`send`.                                      |
| `SO_LINGER`                           |  `struct linger` | Control close behavior (block until sent or drop).                                |
| `SO_ERROR`                            |   `int` (output) | Query pending error on socket (use with `getsockopt`).                            |
| `SO_OOBINLINE`                        |      `int` (0/1) | Receive TCP OOB data inline with normal data.                                     |
| `SO_ACCEPTCONN`                       |   `int` (output) | Check if socket is listening (from `getsockopt`).                                 |
| `SO_DOMAIN`, `SO_TYPE`, `SO_PROTOCOL` |   `int` (output) | Query socket properties.                                                          |

// IPPROTO_TCP: TCP level options
| Option          |      Value type | Purpose                                                                         |
| --------------- | --------------: | ------------------------------------------------------------------------------- |
| `TCP_NODELAY`   |     `int` (0/1) | Disable Nagle (send small packets immediately). Useful for low-latency apps.    |
| `TCP_CORK`      |     `int` (0/1) | Linux: hold back partial frames until cork cleared (for packetization control). |
| `TCP_KEEPIDLE`  | `int` (seconds) | Idle time before first keepalive probe.                                         |
| `TCP_KEEPINTVL` | `int` (seconds) | Interval between keepalive probes.                                              |
| `TCP_KEEPCNT`   |           `int` | Number of probes before declaring connection dead.                              |
| `TCP_SYNCNT`    |           `int` | Number of SYN retransmits before aborting connect() (Linux).                    |
| `TCP_QUICKACK`  |           `int` | Control delayed ACK behavior (Linux).                                           |

// IPPROTO_IP / IP: IPv4 level options
| Option               | Value type / struct       | Purpose                                                  |
| -------------------- | ------------------------- | -------------------------------------------------------- |
| `IP_TTL`             | `int`                     | Set IP time-to-live for outgoing packets.                |
| `IP_MULTICAST_TTL`   | `unsigned char`           | TTL for IPv4 multicast packets.                          |
| `IP_MULTICAST_LOOP`  | `unsigned char`           | Loopback for multicast (0/1).                            |
| `IP_MULTICAST_IF`    | `struct in_addr` or `int` | Choose outgoing interface for multicast.                 |
| `IP_ADD_MEMBERSHIP`  | `struct ip_mreq`          | Join IPv4 multicast group.                               |
| `IP_DROP_MEMBERSHIP` | `struct ip_mreq`          | Leave multicast group.                                   |
| `IP_PKTINFO`         | `int`                     | Receive destination address and iface info in `recvmsg`. |
| `IP_HDRINCL`         | `int` (0/1)               | Include custom IP header when sending (raw sockets).     |

// IPPROTO_IPV6: IPv6 level options
| Option                |         Value type | Purpose                                                          |
| --------------------- | -----------------: | ---------------------------------------------------------------- |
| `IPV6_JOIN_GROUP`     | `struct ipv6_mreq` | Join IPv6 multicast group.                                       |
| `IPV6_LEAVE_GROUP`    | `struct ipv6_mreq` | Leave group.                                                     |
| `IPV6_MULTICAST_HOPS` |              `int` | Multicast hop limit (TTL).                                       |
| `IPV6_V6ONLY`         |              `int` | If set, socket will accept only IPv6 (no IPv4-mapped addresses). |
| `IPV6_PKTINFO`        |              `int` | Similar to `IP_PKTINFO` for IPv6.                                |

// SOL_SOCKET for advanced use
SO_ATTACH_FILTER / SO_DETACH_FILTER — attach BPF filter (raw capture) (Linux).
SO_PASSCRED — get sender credentials on Unix domain sockets.
SO_TIMESTAMP / SO_TIMESTAMPNS / SO_TIMESTAMPING — enable kernel packet timestamps.

// Typical application usage
TCP server that wants fast restart
	SO_REUSEADDR (and sometimes SO_REUSEPORT) before bind().
High-throughput network app
	Increase SO_RCVBUF / SO_SNDBUF and tune TCP window scaling.
Low-latency app (e.g., RPC, games)
	TCP_NODELAY (disable Nagle) + smaller send batching.
Long-lived idle connections (NAT/firewall keepalive)
	SO_KEEPALIVE + TCP_KEEPIDLE / TCP_KEEPINTVL / TCP_KEEPCNT.
UDP multicast receiver
	IP_ADD_MEMBERSHIP to join group, IP_MULTICAST_IF to pick interface.
Raw packet generation
	IP_HDRINCL if you want to provide the IP header.
Non-blocking I/O with timeout fallback
	SO_RCVTIMEO / SO_SNDTIMEO (or set non-blocking + select/poll/epoll).
Graceful close vs force close
	SO_LINGER with l_onoff controls whether close() blocks to send pending data.

```

## hash table
```c
/*
Hash Buckets
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│  0  │  1  │  2  │  3  │ ... │ ... │ ... │ ... │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
   │     │     │     │           │     │     │
   │     │     │     │           │     │     └─► Node → Node → nil
   │     │     │     │           │     └─► nil
   │     │     │     │           └─► Node → nil
   │     │     │     └─► Node → Node → Node → nil
   │     │     └─► nil
   │     └─► Node → nil
   └─► Node → Node → nil
*/
/* -- 1 -- */
/*  hash.c  –  tiny educational hash table  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- tunables ---------- */
#define NUM_BUCKETS 16          /* must be power of two for fast mod */

/* ---------- node in linked list ---------- */
typedef struct Node {
    char *key;
    char *val;
    struct Node *next;
} Node;

/* ---------- the table ---------- */
typedef struct HashTable {
    Node *buckets[NUM_BUCKETS]; /* array of list heads */
} HashTable;

/* ---------- djb2 hash (Kernighan/Pike) ---------- */
static unsigned long hash(const char *str)
{
    unsigned long h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;   /* h * 33 + c */
    return h;
}

/* ---------- create empty table ---------- */
HashTable *ht_create(void)
{
    HashTable *ht = malloc(sizeof *ht);
    for (int i = 0; i < NUM_BUCKETS; ++i)
        ht->buckets[i] = NULL;
    return ht;
}

/* ---------- insert or update ---------- */
void ht_insert(HashTable *ht, const char *key, const char *val)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1); /* fast modulo */
    Node *n = ht->buckets[idx];

    /* update if key already present */
    for (; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            free(n->val);
            n->val = strdup(val);
            return;
        }
    }

    /* create new node and push to front */
    n = malloc(sizeof *n);
    n->key   = strdup(key);
    n->val   = strdup(val);
    n->next  = ht->buckets[idx];
    ht->buckets[idx] = n;
}

/* ---------- search ---------- */
const char *ht_search(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    for (Node *n = ht->buckets[idx]; n; n = n->next)
        if (strcmp(n->key, key) == 0)
            return n->val;
    return NULL;                /* not found */
}

/* ---------- delete ---------- */
void ht_delete(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    Node **link = &ht->buckets[idx]; /* pointer to pointer trick */
    while (*link) {
        Node *n = *link;
        if (strcmp(n->key, key) == 0) {
            *link = n->next;    /* unlink */
            free(n->key);
            free(n->val);
            free(n);
            return;
        }
        link = &n->next;
    }
}

/* ---------- free everything ---------- */
void ht_destroy(HashTable *ht)
{
    for (int i = 0; i < NUM_BUCKETS; ++i) {
        Node *n = ht->buckets[i];
        while (n) {
            Node *next = n->next;
            free(n->key);
            free(n->val);
            free(n);
            n = next;
        }
    }
    free(ht);
}

/* ---------- demo ---------- */
int main(void)
{
    HashTable *ht = ht_create();

    ht_insert(ht, "apple", "red");
    ht_insert(ht, "banana", "yellow");
    ht_insert(ht, "lemon", "yellow");
    ht_insert(ht, "pear", "green");

    printf("banana → %s\n", ht_search(ht, "banana"));
    printf("pear   → %s\n", ht_search(ht, "pear"));
    printf("grape  → %s\n", ht_search(ht, "grape")); /* not found */

    ht_delete(ht, "banana");
    printf("banana after delete → %s\n", ht_search(ht, "banana"));

    ht_destroy(ht);
    return 0;
}

/* -- 2 -- */
/*  hash.c  –  tiny educational hash table  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- tunables ---------- */
#define NUM_BUCKETS 16          /* must be power of two for fast mod */

/* ---------- node in linked list ---------- */
typedef struct Node {
    char       *key;
    void       *value;
    struct Node *next;
} Node;

/* ---------- the table ---------- */
typedef struct {
    Node *buckets[NUM_BUCKETS];
    /* optional user-supplied helpers */
    void *(*value_copy)(const void *);   /* strdup for your type */
    void  (*value_free)(void *);         /* free for your type */
} HashTable;

/* ---------- djb2 hash (Kernighan/Pike) ---------- */
static unsigned long hash(const char *str)
{
    unsigned long h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;   /* h * 33 + c */
    return h;
}

/* ---------- create empty table ---------- */
HashTable *ht_create(void *(*vc)(const void *), void (*vf)(void *))
{
    HashTable *ht = calloc(1, sizeof *ht);
    ht->value_copy = vc;
    ht->value_free = vf;
    return ht;
}

/* ---------- insert or update ---------- */
void ht_insert(HashTable *ht, const char *key, const void *value)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    Node *n;

    /* update existing key */
    for (n = ht->buckets[idx]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            if (ht->value_free) ht->value_free(n->value);
            n->value = ht->value_copy ? ht->value_copy(value) : (void *)value;
            return;
        }
    }

    /* create new node */
    n = malloc(sizeof *n);
    n->key   = strdup(key);
    n->value = ht->value_copy ? ht->value_copy(value) : (void *)value;
    n->next  = ht->buckets[idx];
    ht->buckets[idx] = n;
}

/* ---------- search ---------- */
void *ht_search(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    for (Node *n = ht->buckets[idx]; n; n = n->next)
        if (strcmp(n->key, key) == 0) return n->value;
    return NULL;
}

/* ---------- delete ---------- */
void ht_delete(HashTable *ht, const char *key)
{
    unsigned idx = hash(key) & (NUM_BUCKETS - 1);
    Node **link = &ht->buckets[idx];
    while (*link) {
        Node *n = *link;
        if (strcmp(n->key, key) == 0) {
            *link = n->next;
            free(n->key);
            if (ht->value_free) ht->value_free(n->value);
            free(n);
            return;
        }
        link = &n->next;
    }
}

/* ---------- free everything ---------- */
void ht_destroy(HashTable *ht)
{
    for (int i = 0; i < NUM_BUCKETS; ++i) {
        Node *n = ht->buckets[i];
        while (n) {
            Node *next = n->next;
            free(n->key);
            if (ht->value_free) ht->value_free(n->value);
            free(n);
            n = next;
        }
    }
    free(ht);
}

// 1
void *int_copy(const void *p) { return (void *)p; }

int main(void)
{
    HashTable *ht = ht_create(int_copy, NULL); /* no free needed */

    int k42 = 42, k7 = 7;
    ht_insert(ht, "forty-two", (void *)&k42);
    ht_insert(ht, "seven",     (void *)&k7);

    int v = *(int*)ht_search(ht, "forty-two");
    printf("forty-two = %d\n", v);   /* 42 */

    ht_destroy(ht);
}

// 2
typedef struct {
    double x, y;
} Point;

void *point_copy(const void *src)
{
    Point *p = malloc(sizeof *p);
    *p = *(Point *)src;
    return p;
}
void point_free(void *p) { free(p); }

int main(void)
{
    HashTable *ht = ht_create(point_copy, point_free);

    Point origin = {0.0, 0.0}, unit = {1.0, 1.0};
    ht_insert(ht, "origin", &origin);
    ht_insert(ht, "unit",   &unit);

    Point *u = ht_search(ht, "unit");
    printf("unit  = (%.1f, %.1f)\n", u->x, u->y);

    ht_destroy(ht);
}

// 3
int main(void)
{
    HashTable *ht = ht_create((void *(*)(const void *))strdup, free);

    ht_insert(ht, "apple", "red");
    ht_insert(ht, "banana", "yellow");
    ht_insert(ht, "lemon", "yellow");
    ht_insert(ht, "pear", "green");

    printf("banana → %s\n", (const char *)ht_search(ht, "banana"));
    printf("pear   → %s\n", (const char *)ht_search(ht, "pear"));
    printf("grape  → %s\n", (const char *)ht_search(ht, "grape")); /* not found */

    ht_delete(ht, "banana");
    printf("banana after delete → %s\n", (const char *)ht_search(ht, "banana"));

    ht_destroy(ht);
    return 0;
}

/* -- 3 -- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Node structure for key-value pairs
typedef struct Node {
    char* key;      // e.g., "Alice"
    int value;      // e.g., 30
    struct Node* next;  // For chaining collisions
} Node;

// HashTable structure
typedef struct HashTable {
    Node** buckets;  // Array of pointers to Node
    int size;        // Number of buckets
} HashTable;

// Simple hash function for strings: sum of ASCII % size
int hash(const char* key, int size) {
    int sum = 0;
    for (int i = 0; key[i] != '\0'; i++) {
        sum += key[i];
    }
    return sum % size;
}

// Create a new node
Node* createNode(const char* key, int value) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->key = strdup(key);  // Copy string
    newNode->value = value;
    newNode->next = NULL;
    return newNode;
}

// Initialize hash table
HashTable* createHashTable(int size) {
    HashTable* ht = (HashTable*)malloc(sizeof(HashTable));
    ht->size = size;
    ht->buckets = (Node**)calloc(size, sizeof(Node*));  // Initialize to NULL
    return ht;
}

// Insert or update key-value
void insert(HashTable* ht, const char* key, int value) {
    int index = hash(key, ht->size);
    Node* current = ht->buckets[index];

    // Check if key exists (update value)
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            current->value = value;
            return;
        }
        current = current->next;
    }

    // Key doesn't exist: insert new node at front
    Node* newNode = createNode(key, value);
    newNode->next = ht->buckets[index];
    ht->buckets[index] = newNode;
}

// Search for key and return value (or -1 if not found)
int search(HashTable* ht, const char* key) {
    int index = hash(key, ht->size);
    Node* current = ht->buckets[index];

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return -1;  // Not found
}

// Delete key
void delete(HashTable* ht, const char* key) {
    int index = hash(key, ht->size);
    Node* current = ht->buckets[index];
    Node* prev = NULL;

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            if (prev == NULL) {
                ht->buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            free(current->key);  // Free duplicated string
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Print the entire table (for demo)
void printTable(HashTable* ht) {
    for (int i = 0; i < ht->size; i++) {
        printf("Bucket %d: ", i);
        Node* current = ht->buckets[i];
        while (current != NULL) {
            printf("(%s=%d) -> ", current->key, current->value);
            current = current->next;
        }
        printf("NULL\n");
    }
}

// Free the hash table (cleanup)
void freeHashTable(HashTable* ht) {
    for (int i = 0; i < ht->size; i++) {
        Node* current = ht->buckets[i];
        while (current != NULL) {
            Node* temp = current;
            current = current->next;
            free(temp->key);
            free(temp);
        }
    }
    free(ht->buckets);
    free(ht);
}

int main() {
    HashTable* ht = createHashTable(10);

    // Insert some data
    insert(ht, "Alice", 30);
    insert(ht, "Bob", 25);
    insert(ht, "Charlie", 35);  // Might collide with Alice depending on hash
    insert(ht, "David", 28);

    // Print table
    printf("Hash Table Contents:\n");
    printTable(ht);

    // Search example
    int age = search(ht, "Bob");
    printf("\nBob's age: %d\n", age);

    // Delete example
    delete(ht, "Alice");
    printf("\nAfter deleting Alice:\n");
    printTable(ht);

    // Cleanup
    freeHashTable(ht);
    return 0;
}
```

```lua
-- 1
-- Simple hash function for ints: key % size (handle negative)
local function hash(key, size)
    return ((key % size) + size) % size
end

-- Create a new node table
local function createNode(key, value)
    return {key = key, value = value, next = nil}
end

-- Initialize hash table
local function createHashTable(size)
    local ht = {size = size, buckets = {}}
    for i = 1, size do
        ht.buckets[i] = nil  -- Empty buckets
    end
    return ht
end

-- Insert or update key-value
local function insert(ht, key, value)
    local index = hash(key, ht.size)
    local current = ht.buckets[index]

    -- Check if key exists (update value)
    while current ~= nil do
        if current.key == key then
            current.value = value
            return
        end
        current = current.next
    end

    -- Key doesn't exist: insert new node at front
    local newNode = createNode(key, value)
    newNode.next = ht.buckets[index]
    ht.buckets[index] = newNode
end

-- Search for key and return value (or -1 if not found)
local function search(ht, key)
    local index = hash(key, ht.size)
    local current = ht.buckets[index]

    while current ~= nil do
        if current.key == key then
            return current.value
        end
        current = current.next
    end
    return -1  -- Not found
end

-- Delete key
local function delete(ht, key)
    local index = hash(key, ht.size)
    local current = ht.buckets[index]
    local prev = nil

    while current ~= nil do
        if current.key == key then
            if prev == nil then
                ht.buckets[index] = current.next
            else
                prev.next = current.next
            end
            -- No explicit free; Lua GC handles it
            return
        end
        prev = current
        current = current.next
    end
end

-- Print the entire table (for demo)
local function printTable(ht)
    for i = 1, ht.size do
        io.write("Bucket " .. (i - 1) .. ": ")
        local current = ht.buckets[i]
        while current ~= nil do
            io.write("(" .. current.key .. "=" .. current.value .. ") -> ")
            current = current.next
        end
        print("nil")
    end
end

-- Main demo
local ht = createHashTable(10)

-- Insert some data (int keys: e.g., employee IDs)
insert(ht, 123, 30)   -- ID 123, age 30 (123 % 10 = 3)
insert(ht, 133, 25)   -- ID 133 hashes to 3
insert(ht, 143, 35)   -- ID 143 hashes to 3 (collision with 133!)
insert(ht, 100, 28)   -- ID 100 hashes to 0

-- Print table
print("Hash Table Contents:")
printTable(ht)

-- Search example
local age = search(ht, 133)
print("\nID 133's age: " .. age)

-- Delete example
delete(ht, 123)
print("\nAfter deleting ID 123:")
printTable(ht)

-- OOP style
-- HashTable prototype (methods)
local HashTable = {}
HashTable.__index = HashTable

-- Simple hash function for ints: key % size (handle negative)
local function hash(key, size)
    return ((key % size) + size) % size
end

-- Constructor: Create a new HashTable instance
function HashTable.new(size)
    local self = {
        size = size,
        buckets = {}
    }
    -- Initialize empty buckets
    for i = 1, size do
        self.buckets[i] = nil
    end
    -- Set metatable for method access
    setmetatable(self, HashTable)
    return self
end

-- Create a new node table
local function createNode(key, value)
    return {key = key, value = value, next = nil}
end

-- Insert or update key-value
function HashTable:insert(key, value)
    local index = hash(key, self.size)
    local current = self.buckets[index]

    -- Check if key exists (update value)
    while current ~= nil do
        if current.key == key then
            current.value = value
            return
        end
        current = current.next
    end

    -- Key doesn't exist: insert new node at front
    local newNode = createNode(key, value)
    newNode.next = self.buckets[index]
    self.buckets[index] = newNode
end

-- Search for key and return value (or -1 if not found)
function HashTable:search(key)
    local index = hash(key, self.size)
    local current = self.buckets[index]

    while current ~= nil do
        if current.key == key then
            return current.value
        end
        current = current.next
    end
    return -1  -- Not found
end

-- Delete key
function HashTable:delete(key)
    local index = hash(key, self.size)
    local current = self.buckets[index]
    local prev = nil

    while current ~= nil do
        if current.key == key then
            if prev == nil then
                self.buckets[index] = current.next
            else
                prev.next = current.next
            end
            -- No explicit free; Lua GC handles it
            return
        end
        prev = current
        current = current.next
    end
end

-- Print the entire table (for demo)
function HashTable:printTable()
    for i = 1, self.size do
        io.write("Bucket " .. (i - 1) .. ": ")
        local current = self.buckets[i]
        while current ~= nil do
            io.write("(" .. current.key .. "=" .. current.value .. ") -> ")
            current = current.next
        end
        print("nil")
    end
end

-- Main demo
local ht = HashTable.new(10)

-- Insert some data (int keys: e.g., employee IDs)
ht:insert(123, 30)   -- ID 123, age 30 (123 % 10 = 3)
ht:insert(133, 25)   -- ID 133 hashes to 3
ht:insert(143, 35)   -- ID 143 hashes to 3 (collision with 133!)
ht:insert(100, 28)   -- ID 100 hashes to 0

-- Print table
print("Hash Table Contents:")
ht:printTable()

-- Search example
local age = ht:search(133)
print("\nID 133's age: " .. age)

-- Delete example
ht:delete(123)
print("\nAfter deleting ID 123:")
ht:printTable()
```

## ring buffer
[Ring-Buffer](https://github.com/AndersKaloer/Ring-Buffer/tree/master)  
[c-ringbuf](https://github.com/dhess/c-ringbuf/tree/master)  
[]()  
[]()  
[]()  
[]()  
```c
# 1
#include <stdio.h>

#define BUFFER_SIZE 5

typedef struct {
    int data[BUFFER_SIZE];
    int count; // Number of elements in the buffer (0 to BUFFER_SIZE)
    int tail;  // Index where the next element will be inserted
} RingBuffer;

// Add a new integer to the ring buffer
void add(RingBuffer *buf, int value) {
    buf->data[buf->tail] = value;
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;

    if (buf->count < BUFFER_SIZE) {
        buf->count++;
    }
}

// Print the buffer contents from oldest to newest
void print_buffer(const RingBuffer *buf) {
    int start = (buf->tail - buf->count + BUFFER_SIZE) % BUFFER_SIZE;
    for (int i = 0; i < buf->count; i++) {
        int idx = (start + i) % BUFFER_SIZE;
        printf("%d ", buf->data[idx]);
    }
    printf("\n");
}

// Main: test the ring buffer
int main() {
    RingBuffer buf = { .count = 0, .tail = 0 };

    for (int i = 1; i <= 8; ++i) {
        add(&buf, i);
        printf("After adding %d: ", i);
        print_buffer(&buf);
    }

    return 0;
}

# 2
#include <stdio.h>

#define BUFFER_SIZE 5

typedef struct {
    int data[BUFFER_SIZE];
    int count; // Number of elements in the buffer (0 to BUFFER_SIZE)
    int head;  // Index of the oldest element
} RingBuffer;

// Add a new integer to the ring buffer
void add(RingBuffer *buf, int value) {
    int insert_index = (buf->head + buf->count) % BUFFER_SIZE;

    buf->data[insert_index] = value;

    if (buf->count < BUFFER_SIZE) {
        buf->count++;
    } else {
        // Buffer full, move head to next oldest
        buf->head = (buf->head + 1) % BUFFER_SIZE;
    }
}

// Print the buffer contents from oldest to newest
void print_buffer(const RingBuffer *buf) {
    for (int i = 0; i < buf->count; i++) {
        int idx = (buf->head + i) % BUFFER_SIZE;
        printf("%d ", buf->data[idx]);
    }
    printf("\n");
}

// Main: test the ring buffer
int main() {
    RingBuffer buf = { .count = 0, .head = 0 };

    for (int i = 1; i <= 8; ++i) {
        add(&buf, i);
        printf("After adding %d: ", i);
        print_buffer(&buf);
    }

    return 0;
}

# 3
#include<stdio.h>
#include<malloc.h>
#include<memory.h>
#include<unistd.h>
#include<stdlib.h>

typedef struct cbuff_{
    int * buff;
    int start;
    int end;
    int size;
    int count;
} cbuff_t;

cbuff_t* cbuff_new(int size)
{
  cbuff_t *cb = (cbuff_t*)malloc(sizeof(cbuff_t));
  memset(cb, 0, sizeof(cbuff_t));
  cb->size = size;
    cb->buff = (int*)malloc(sizeof(int)*size);
  
  return cb;
}

void cbuff_add(cbuff_t *cb, int elem)
{
  int end = cb->end;
  if(cb->count && (end % cb->size) == cb->start) {
    printf("Overflow Elem[%d] %d lost\n", cb->start, cb->buff[cb->start]);
    cb->start = (cb->start + 1 ) %cb->size;
    cb->count --;
  }

  printf("Added Elem[%d] = %d\n",cb->end, elem);
  cb->buff[cb->end] = elem;
  cb->end = (cb->end+1) % cb->size;
  cb->count ++;
}

int cbuff_remove(cbuff_t *cb)
{
  int start = cb->start ;
  int ret = -1;
  if(cb->count <= 0) {
    printf("Buffer is empty\n");
    return ret;
  }

  if(cb->count || (start % cb->size) != cb->end) {
    printf("Removed Elem[%d] = %d\n",cb->start, cb->buff[cb->start]);
    ret = cb->buff[cb->start];
    cb->start = (cb->start + 1 ) % cb->size;
    cb->count--;
  } else {
    printf("Buffer is empty\n");
  }

  return ret;
}

void cbuff_print(cbuff_t *cb)
{
  int start = cb->start ;
  int end = cb->end ;
  int i, count = 0;
  for(i = start; count < cb->count; i = (i + 1) % cb->size) {
    printf("Elem[%d] = %d\n", i, cb->buff[i]);
    count++;
    if(i == (end - 1)) {
      break;
    }
  }
}

void cbuff_delete(cbuff_t *cb)
{
  free(cb->buff);
  free(cb);
}

int main(int argc, char *argv[])
{
  char key;
  int elem;
  cbuff_t *cb = cbuff_new(5);

  while(1) {
    printf("circular buffer add[a], remove[r], print[p] : ");
    fflush(stdin);

    key = getchar();
    switch(key) {
    case 'a':
      printf("Element to add : ");
      scanf("%d", &elem);
      cbuff_add(cb, elem);
      break;
    case 'r':
      cbuff_remove(cb);
      break;
    case 'p':
      cbuff_print(cb);
      break;
    case 'q':
      cbuff_delete(cb);
      exit(0);
    }

    continue;
  }

  return 0;
}

# 4
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 5

typedef struct RingBuffer	RingBuffer;
struct RingBuffer
{
	int	count;	/* occupied size of data[]*/
	int	tail;	  /* index of last entry inserted + 1 */
	int data[BUFFER_SIZE];
};

void ringbuffer_add(RingBuffer *rb, int value)
{
    rb->data[rb->tail] = value;
    rb->tail = (rb->tail + 1) % BUFFER_SIZE;
    if (rb->count < BUFFER_SIZE) {
        rb->count++;
    }
}

void ringbuffer_print(RingBuffer *rb)
{
    if(rb->count == 0) {
        printf("RingBuffer is empty.\n");
        return;
    }
  
  int start = (rb->tail - rb->count + BUFFER_SIZE) % BUFFER_SIZE;
    for(int i = 0; i < rb->count; i++) {
        int idx = (start + i) % BUFFER_SIZE;
        printf("%d ", rb->data[idx]);
    }

    printf("\n");
}

int main(void)
{
    int i;
    RingBuffer rb;

    memset(&rb, 0, sizeof(rb));
    ringbuffer_print(&rb);

    for (i = 0; i < 10; i++) {
        ringbuffer_add(&rb, i);
        ringbuffer_print(&rb);
    }

    return 0;
}
```

## C library
[ctl](https://github.com/glouw/ctl)  
> C TEMPLATE LIBRARY (CTL) is a fast compiling, type safe, header only, template-like library for ISO C99/C11.
[C CONTAINER TEMPLATE LIBRARY (CTL)](https://rurban.github.io/ctl/)  
[]()  
[]()  
[]()  
[]()  

## pthread
[Multithreaded Programming (POSIX pthreads Tutorial)](https://randu.org/tutorials/threads/)  
[POSIX Threads API](https://www.cs.fsu.edu/~baker/realtime/restricted/notes/pthreads.html)  
[POSIX thread (pthread) libraries](https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  


## 动态链接 和 静态链接
[**Program Library HOWTO**](https://tldp.org/HOWTO/Program-Library-HOWTO/index.html)  
[C++ dlopen mini HOWTO](https://tldp.org/HOWTO/C++-dlopen/)  
[Shared libraries with GCC on Linux](https://www.cprogramming.com/tutorial/shared-libraries-linux-gcc.html)  
[]()  
[]()  
[How dynamic linking for modular libraries works on Linux](https://opensource.com/article/22/5/dynamic-linking-modular-libraries-linux)  
[How to handle dynamic and static libraries in Linux](https://opensource.com/article/20/6/linux-libraries)  
```c
When we link an application against a shared library, the linker leaves some stubs (unresolved symbols) which need to be filled by dynamic linker at run time or at application loading time.

Loading of a shared library is of two types::
1. Dynamically linked libraries
2. Dynamically loaded libraries

// Dynamically linked libraries
gcc -c -Wall -Werror -fpic foo.c		// Compiling with Position Independent Code
gcc -shared -o libfoo.so foo.o			// Creating a shared library from an object file
gcc -L/home/username/foo -Wall -o test main.c -lfoo				// Linking with a shared library

// 1 use LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/home/username/foo:$LD_LIBRARY_PATH		// Making the library available at runtime
./test

// 2 use rpath
unset LD_LIBRARY_PATH
gcc -L/home/username/foo -Wl,-rpath=/home/username/foo -Wall -o test main.c -lfoo
./test

//  Dynamically loaded libraries
Useful for creating a "plug-in" architecture. 
The program takes full control by calling functions with the library. This is done using dlopen(), dlsym(), dlclose().


// 动态链接
// Locating a shared object during compilation
gcc -I ./include -c src/demo.c				// -I option: adds a directory to GCC's search path for header files

gcc -L`pwd`/lib -o myDemo demo.o -lexample	// -L option: adds an additional library path to GCC's search locations. 
											// -l: sets the name of the library you want to link against

ldd ./myDemo								// ldd: prints shared object dependencies
        linux-vdso.so.1 (0x00007ffe151df000)
        libexample.so => not found
        libc.so.6 => /lib64/libc.so.6 (0x00007f514b60a000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f514b839000)

LD_LIBRARY_PATH=`pwd`/lib ldd ./			// LD_LIBRARY_PATH: the environment variable which defines the path to libraries
   linux-vdso.so.1 (0x00007ffe515bb000)
   libexample.so => /tmp/Demo/lib/libexample.so (0x0000...
   libc.so.6 => /lib64/libc.so.6 (0x00007eff037ee000)
   /lib64/ld-linux-x86-64.so.2 (0x00007eff03a22000)

LD_LIBRARY_PATH=`pwd`/lib ./myDemo			// 

// When to use LD_LIBRARY_PATH
/*
	1. compiling software that needs to link against a library that itself has just been compiled and has not yet been installed
	2. bundling software that's designed to run out of a single directory, with no install script or an install script that places libraries in non-standard directories
*/

// 静态链接
A static library is an archive (ar) of object files

file libmy_static.a		// libmy_static.a: current ar archive

ar -t libmy_static.a 	// look into the archive

ar -x libmy_static.a	// extract the archive's files
```

## libraries
### ini parser
[iniparser](https://github.com/ndevilla/iniparser/tree/main)  
[inih](https://github.com/benhoyt/inih)  
[Notcurses: blingful TUIs and character graphics](https://github.com/dankamongmen/notcurses)  

## 函数指针的应用场景
```c
// 1 回调机制
#include <stdio.h>

typedef struct Button {
    void (*onClick)();  // 回调函数指针
} Button;

void buttonClicked() {
    printf("Button was clicked!\n");
}

int main() {
    Button btn;
    btn.onClick = buttonClicked;  // 注册回调函数
    if (btn.onClick) {
        btn.onClick();  // 调用回调函数
    }
    return 0;
}

// callback function
int compare(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

qsort(array, size, sizeof(int), compare);


// 2 策略模式
#include <stdio.h>

typedef struct Sorter {
    int (*compare)(int, int);  // 函数指针定义了比较策略
} Sorter;

int ascending(int a, int b) {	// 比较策略 1
    return a - b;
}

int descending(int a, int b) {	// 比较策略 2
    return b - a;
}

void sort(int arr[], int n, Sorter sorter) {	// 动态指定比较策略
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (sorter.compare(arr[j], arr[j + 1]) > 0) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

int main() {
    int arr[] = {5, 3, 7, 2, 8};
    Sorter ascSorter = {ascending};
    Sorter descSorter = {descending};
    
    sort(arr, 5, ascSorter);	// 动态指定比较策略 1
    printf("Ascending: ");
    for (int i = 0; i < 5; i++) printf("%d ", arr[i]);
    printf("\n");
    
    sort(arr, 5, descSorter);	// 动态指定比较策略 2
    printf("Descending: ");
    for (int i = 0; i < 5; i++) printf("%d ", arr[i]);
    printf("\n");
    
    return 0;
}

// 3 插件系统
#include <stdio.h>

typedef struct Filter {
    void (*apply)(int*, int);  // 函数指针指向具体的处理函数
} Filter;

void grayscale(int* pixels, int size) {
    printf("Applying grayscale filter\n");
}

void sepia(int* pixels, int size) {
    printf("Applying sepia filter\n");
}

int main() {
    int pixels[100];
    Filter filters[] = {	// 数组存储多个Filter对象，每个对象关联一个Filter函数
        {grayscale},
        {sepia}
    };
    
    for (int i = 0; i < 2; i++) {		// 遍历数组并调用apply，实现插件式的Filter应用
        filters[i].apply(pixels, 100);
    }
    return 0;
}

// 4 状态机
#include <stdio.h>

typedef struct State {
    void (*handleInput)(char);  // 函数指针处理特定状态下的输入
} State;

void handleListen(char input) {
    if (input == 'S') printf("Received SYN, moving to SYN_RECEIVED\n");
}

void handleSynReceived(char input) {
    if (input == 'A') printf("Received ACK, moving to ESTABLISHED\n");
}

int main() {
    State states[] = {		  // 每个State（如LISTEN和SYN_RECEIVED）有独立的处理逻辑
        {handleListen},
        {handleSynReceived}
    };
    
    states[0].handleInput('S');  // 状态机在不同状态间的转换
    states[1].handleInput('A');  // 状态机在不同状态间的转换
    return 0;
}

//
#include <stdio.h>

typedef enum {
    STATE_RED,
    STATE_GREEN,
    STATE_YELLOW,
    STATE_COUNT
} TrafficLightState;

typedef TrafficLightState (*StateFunction)();

TrafficLightState red_state() {
    printf("State: RED\n");
    return STATE_GREEN;
}

TrafficLightState green_state() {
    printf("State: GREEN\n");
    return STATE_YELLOW;
}

TrafficLightState yellow_state() {
    printf("State: YELLOW\n");
    return STATE_RED;
}

int main() {
    StateFunction state_table[STATE_COUNT] = {
        red_state,
        green_state,
        yellow_state
    };

    TrafficLightState current_state = STATE_RED;

    for (int i = 0; i < 10; ++i) {
        current_state = state_table[current_state]();
    }

    return 0;
}


// 5 对象模拟
#include <stdio.h>

typedef struct Animal {
    void (*speak)();  // 函数指针模拟类的“方法”
} Animal;

void dogSpeak() {		// 不同的函数指针实现各自的行为
    printf("Woof!\n");
}

void catSpeak() {		// 不同的函数指针实现各自的行为
    printf("Meow!\n");
}

int main() {
    Animal dog = {dogSpeak};
    Animal cat = {catSpeak};
    
    dog.speak();
    cat.speak();
    return 0;
}

// 6 事件分发
#include <stdio.h>

typedef struct EventHandler {
    void (*handle)(int eventType);  // 函数指针用于事件处理
} EventHandler;

void handleEvent(int eventType) {	// 根据eventType执行不同逻辑
    if (eventType == 1) {
        printf("Handling event type 1\n");
    } else if (eventType == 2) {
        printf("Handling event type 2\n");
    }
}

int main() {
    EventHandler handler = {handleEvent};
    
    handler.handle(1);	// 分发事件，实现集中式事件处理
    handler.handle(2);	// 分发事件，实现集中式事件处理
    return 0;
}

// 7 线程池
#include <stdio.h>

typedef struct Task {			// 结构体包含execute函数指针和参数arg
    void (*execute)(void* arg);
    void* arg;
} Task;

void printNumber(void* arg) {	// 任务函数，接收参数并执行
    int num = *(int*)arg;
    printf("Number: %d\n", num);
}

int main() {
    int num = 42;
    Task task = {printNumber, &num};
    
    task.execute(task.arg);  // 执行线程池中的任务
    return 0;
}

// 8 虚拟函数表
#include <stdio.h>

typedef struct VTable {		// 虚函数表定义(包含函数指针)
    void (*speak)();
} VTable;

typedef struct Animal {		// 结构体通过函数表指针关联具体的实现
    VTable* vtable;
} Animal;

void dogSpeak() {
    printf("Woof!\n");
}

void catSpeak() {
    printf("Meow!\n");
}

VTable dogVTable = {dogSpeak};
VTable catVTable = {catSpeak};

int main() {
    Animal dog = {&dogVTable};	// 通过不同的虚函数表实现多态调用
    Animal cat = {&catVTable};	// 通过不同的虚函数表实现多态调用
    
    dog.vtable->speak();
    cat.vtable->speak();
    return 0;
}

// 9 钩子函数
#include <stdio.h>

typedef struct Monitor {				// 结构体中的函数指针注册钩子函数
    void (*onChange)(const char* file);
} Monitor;

void logChange(const char* file) {		// 钩子函数实现
    printf("File changed: %s\n", file);
}

int main() {
    Monitor monitor = {logChange};
    
    if (monitor.onChange) {
        monitor.onChange("example.txt");  // 触发钩子函数逻辑
    }
    return 0;
}

// 10 从动态库加载函数
#include <stdio.h>
#include <dlfcn.h>

typedef struct Plugin {
    void (*init)();
    void (*cleanup)();
} Plugin;

int main() {
    void* handle = dlopen("./libplugin.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    
    Plugin plugin;
    plugin.init = dlsym(handle, "plugin_init");			// dlsym获取函数地址
    plugin.cleanup = dlsym(handle, "plugin_cleanup");	// dlsym获取函数地址
    
    if (plugin.init)
		plugin.init();

    if (plugin.cleanup)
		plugin.cleanup();
    
    dlclose(handle);
    return 0;
}

// jump tables
void add() { printf("Add\n"); }
void subtract() { printf("Subtract\n"); }

void (*operation_table[2])() = { add, subtract };
operation_table[op_index]();  // Dynamically invoke the correct operation

// Plugin-like Architecture / Dynamic Behavior
typedef struct {
    void (*init)();
    void (*process)();
} Module;

void mod1_init() { printf("Mod1 Init\n"); }
void mod1_process() { printf("Mod1 Process\n"); }

Module mod1 = { mod1_init, mod1_process };
mod1.init();
mod1.process();

// Encapsulating Behavior in Data Structures
typedef struct {
    void (*draw)(void*);
} Shape;

void draw_circle(void* s) { printf("Drawing Circle\n"); }

Shape circle = { draw_circle };
circle.draw(&circle);

```

## 常用函数
```c
ioctl, fcntl
mmap, munmap
fopen, fread, fwrite, fclose, ferror
popen, pclose, fdopen
open, read, write, close
fprintf,  sprintf, snprintf
fscanf, scanf
strchr, strrchr, strstr, strtok, strtok_r
strdup
strlen
access
strncmp, strcasecmp
fgets
system
isalpha, isxdigit
atoi, strtoul, strtol
toupper, tolower
malloc, free
getenv
regcomp, regexec, regfree
fork, execve
waitpid
raise
memset, memmove, memcpy
kill
reboot
select, poll, epoll
socket, getsockopt, setsockopt
pthread_mutex_init, pthread_mutex_destroy
pthread_mutex_lock， pthread_mutex_unlock
pthread_mutexattr_init, pthread_mutexattr_setpshared
shm_open, shm_unlink
truncate, ftruncate
pthread_self, pthread_setaffinity_np, pthread_getaffinity_np
strerror

void *memcpy(void *dest, const void *src, size_t n);
The memcpy() function copies n bytes from memory area src to memory area dest. The memory areas must not overlap.  Use memmove(3) if the memory areas do overlap.

void *memmove(void *dest, const void *src, size_t n);
The  memmove()  function  copies n bytes from memory area src to memory area dest. The memory areas may overlap: copying takes place as though the bytes in src are first copied into a temporary array that does not overlap src or dest, and the bytes are then copied from the temporary array to dest.

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
	struct pollfd {
		int   fd;         /* file descriptor */
		short events;     /* requested events */
		short revents;    /* returned events */
	};
The caller should specify the number of items in the fds array in nfds.
The field fd contains a file descriptor for an open file.
The field events is an input parameter, a bit mask specifying the events the application is interested in for the file descriptor fd.
The field revents is an output parameter, filled by the kernel with the events that actually occurred. 

// strerror
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(void)
{
		FILE *fp;
		char filename[128];

		snprintf(filename, sizeof(filename), "test.txt");
		fp = fopen(filename, "r");
		if(fp == NULL) {
						printf("fopen %s failed: %s\n", filename, strerror(errno));
						//printf("fopen %s failed: %m\n", filename);
		}

		return 0;
}

// waitpid
// return immediately if no child has exited
pid = waitpid(0, &status, WNOHANG);
if(pid <= 0)
	return;

// raise (发送信号给当前进程自己)
#include <signal.h>

void sig_handler(int signo, siginfo_t *info, void *ucontext)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	// inside first-level signal handler, set signal handler to default action
	act.sa_handler = SIG_DFL;

	sigaction(signo, &act, NULL);

	/* Take appropriate actions according to needs */
	......

	// send a signal to the caller, 在本例中会触发系统的默认的信号处理
	raise(signo);
}

int set_sighandler()
{
	int r;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sig_handler;
	sa.sa_flags = SA_SIGINFO;

	// assume signo is SIGUSR1
	r = sigaction(SIGUSR1, &sa, NULL);
	if(r < 0) {
		printf("sigaction failed: %s", strerror(errno));
		return -1;
	}
	/* Further code */

		return 0;
}

// statvfs (获取文件系统统计信息)
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>

#define SWM_TMP_SPACE 200

int
chk_tmp_space(void)
{
	int r;
	int free;
	struct statvfs vfs;

	memset(&vfs, 0, sizeof(vfs));
	r = statvfs("/", &vfs);
	if(r) {
		printf("statvfs failed: %s", strerror(errno));
		return -1;
	}

	free = (vfs.f_bsize * vfs.f_bfree) / (1024 * 1024);

	printf("tmp space required %dMB free %dMB\n",SWM_TMP_SPACE, free);

	if(free < SWM_TMP_SPACE) {
		printf("Not enough space, required %dMB, available %dMB\n", SWM_TMP_SPACE, free);
		return -1;
	}

	return 0;
}

// popen 解析shell命令执行结果，并在程序中进行使用
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

enum {
	/* lacp churn machine states */
	CHURN_MONITOR	= 0,	/* monitoring for churn */
	CHURNED		= 1,	/* churn detected (error) */
	NO_CHURN	= 2,	/* no churn (no error) */

	BLK_AGGR	= 0,	/* parsing aggregator info */
	BLK_S1		= 1,	/* parsing slave 1 (fm1-mac1.p1) info */
	BLK_S1_ACTOR	= 2,	/* parsing slave 1 actor lacp pdu */
	BLK_S1_PARTNER	= 3,	/* parsing slave 1 partner lacp pdu */
	BLK_S2		= 4,	/* parsing slave 2 (fm1-mac1.p1) info */
	BLK_S2_ACTOR	= 5,	/* parsing slave 2 actor lacp pdu */
	BLK_S2_PARTNER	= 6,	/* parsing slave 2 partner lacp pdu */
	};

typedef struct slave_status_t slave_status_t;
struct slave_status_t
{
	int	actor_system_prio;
	int	actor_port_key;
	int	actor_port_prio;
	int	actor_port_number;
	int	actor_port_state;
	int	actor_churn_state;
	int	actor_churn_count;
	int	partner_system_prio;
	int	partner_oper_key;
	int	partner_port_prio;
	int	partner_port_number;
	int	partner_port_state;
	int	partner_churn_state;
	int	partner_churn_count;
};

void print_slave_status(slave_status_t *slave)
{
	printf("actor_system_prio:%d\n", slave->actor_system_prio);
	printf("actor_port_key:%d\n", slave->actor_port_key);
	printf("actor_port_prio:%d\n", slave->actor_port_prio);
	printf("actor_port_number:%d\n", slave->actor_port_number);
	printf("actor_port_state:%d\n", slave->actor_port_state);
	printf("actor_churn_state:%d\n", slave->actor_churn_state);
	printf("actor_churn_count:%d\n", slave->actor_churn_count);
	printf("partner_system_prio:%d\n", slave->partner_system_prio);
	printf("partner_oper_key:%d\n", slave->partner_oper_key);
	printf("partner_port_prio:%d\n", slave->partner_port_prio);
	printf("partner_port_number:%d\n", slave->partner_port_number);
	printf("partner_port_state:%d\n", slave->partner_port_state);
	printf("partner_churn_state:%d\n", slave->partner_churn_state);
	printf("partner_churn_count:%d\n", slave->partner_churn_count);
}

int main(void)
{
	FILE *fp;
	int blk, val;
	const char delim[] = ":";
	slave_status_t	slaves[2], *s1, *s2;
	char cmd[128], line[256], *fstr, *vstr;

	s1 = &slaves[0];
	s2 = &slaves[1];

	snprintf(cmd, sizeof(cmd), "cat bond0.txt");

	fp = popen(cmd, "r");
	if(fp == NULL) {
		printf("popen failed: %s\n", strerror(errno));
		return -1;
	}

	blk = BLK_AGGR;
	while(fgets(line, sizeof(line), fp) != NULL) {
		if(strstr(line, delim) == NULL)
			continue;
		
		fstr = strtok(line, delim);
		vstr = strtok(NULL, "");

		if((fstr == NULL) ||
			((vstr == NULL) &&
			(strcasestr(fstr, "details actor lacp pdu") == NULL) &&
			(strcasestr(fstr, "details partner lacp pdu") == NULL)))
			continue;
		
		if(strcasestr(fstr, "Slave Interface") &&
			strcasestr(vstr, "fm1-mac1.p1")) {
				blk = BLK_S1;
			}
		else if(strcasestr(fstr, "Slave Interface") &&
			strcasestr(vstr, "fm1-mac1.p2")) {
			blk = BLK_S2;
		}
		else if(strcasestr(fstr, "details actor lacp pdu")) {
			if(blk == BLK_S1)
				blk = BLK_S1_ACTOR;
			else if(blk == BLK_S2)
				blk = BLK_S2_ACTOR;
		}
		else if(strcasestr(fstr, "details partner lacp pdu")) {
			if(blk == BLK_S1_ACTOR)
				blk = BLK_S1_PARTNER;
			else if(blk == BLK_S2_ACTOR)
				blk = BLK_S2_PARTNER;
		}
		else if(strcasestr(fstr, "Actor Churn State")) {
			if(strcasestr(vstr, "monitoring"))
				val = CHURN_MONITOR;
			else if(strcasestr(vstr, "churned"))
				val = CHURNED;
			else
				val = NO_CHURN;

			if(blk == BLK_S1)
				s1->actor_churn_state = val;
			else if(blk == BLK_S2)
				s2->actor_churn_state = val;
		}
		else if(strcasestr(fstr, "Partner Churn State")) {
			if(strcasestr(vstr, "monitoring"))
				val = CHURN_MONITOR;
			else if(strcasestr(vstr, "churned"))
				val = CHURNED;
			else
				val = NO_CHURN;

			if(blk == BLK_S1)
				s1->partner_churn_state = val;
			else if(blk == BLK_S2)
				s2->partner_churn_state = val;
		}
		else if(strcasestr(fstr, "Actor Churned Count")) {
			if(blk == BLK_S1)
				s1->actor_churn_count = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2)
				s2->actor_churn_count = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "Partner Churned Count")) {
			if(blk == BLK_S1)
				s1->partner_churn_count = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2)
				s2->partner_churn_count = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "system priority")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_system_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S1_PARTNER)
				s1->partner_system_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_system_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_system_prio = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "port key")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_port_key = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_port_key = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "oper key")) {
			if(blk == BLK_S1_PARTNER)
				s1->partner_oper_key = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_oper_key = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "port priority")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_port_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S1_PARTNER)
				s1->partner_port_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_port_prio = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_port_prio = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "port number")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_port_number = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S1_PARTNER)
				s1->partner_port_number = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_port_number = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_port_number = strtoul(vstr, NULL, 0);
		}
		else if(strcasestr(fstr, "port state")) {
			if(blk == BLK_S1_ACTOR)
				s1->actor_port_state = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S1_PARTNER)
				s1->partner_port_state = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_ACTOR)
				s2->actor_port_state = strtoul(vstr, NULL, 0);
			else if(blk == BLK_S2_PARTNER)
				s2->partner_port_state = strtoul(vstr, NULL, 0);
		}
	}

	print_slave_status(s1);
	printf("----------------------------\n");
	print_slave_status(s2);

	pclose(fp);

	return 0;
}

// strtok 可以指定多个分隔符, strtok 内部维护 last token 的位置，因此 strtok 操作的字符串必须保存在 buffer 数组里，因为 strtok 会原地修改数组内容
// 以 `char buf[] = "this,is,a,string";`为例，连续调用 strtok 之后 buffer 数组的内容如下
	t  h  i  s  ,  i  s  ,  a  ,  s  t  r  i  n  g \0         this,is,a,string
	
	t  h  i  s  \0 i  s  ,  a  ,  s  t  r  i  n  g \0         this
	^
	t  h  i  s  \0 i  s  \0 a  ,  s  t  r  i  n  g \0         is
					^
	t  h  i  s  \0 i  s  \0 a  \0 s  t  r  i  n  g \0         a
							^
	t  h  i  s  \0 i  s  \0 a  \0 s  t  r  i  n  g \0         string

// scanf, fscanf, sscanf, vscanf, vsscanf, vfscanf
// scanf使用空白符(newlines, tabs, and spaces)将输入分割成各个不同的域(scanf matches up consecutive conversion specifications to consecutive fields, skipping over the whitespace in between)
[C scanf format specifier](https://www.demo2s.com/c/c-scanf-format-specifier.html)  
[带过滤器的scanf读取字符和字符串](https://www.demo2s.com/c/c-reading-characters-and-string-using-scanf-with-filter.html)  

// kill
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

int check_process_stop(int pid)
{
	int r, status;

	r = waitpid(pid, &status, WNOHANG);
	if(r == 0)
		return 1;

	if(r < 0) {
		printf("waitpid failed: %s\n", strerror(errno));
		return 1;
	}

	printf("process %d exited: r = %d\n", pid, r);

	return 0;
}

int main(void)
{
	int n, i, r, ai, my_pid;
	const char *av[10];
	char cmd[256];

	av[0] = "/mnt/c/Users/morrism/Downloads/test.sh";
	av[1] = "param1";
	ai = 2;
	av[ai++] = "param2";
	av[ai] = NULL;

	n = 0;
	for(i = 0; i < ai; i++)
		n += snprintf(cmd + n, sizeof(cmd) - n, " %s", av[i]);
	printf("cmd:%s\n", cmd);

	my_pid = fork();
	switch(my_pid) {
	default:
		// in parent process, doSomething such as monitoring
		printf("This is parent process\n");
		break;
	case -1:
		printf("fork failed: %s\n", strerror(errno));
		my_pid = 0;
		break;
	case 0:
		setpgid(0, 0);
		execv(av[0], (char*const*)av);
		exit(-1);
	}

		// sleep 5 secs for test purpose
	sleep(5);

	// tear down child process
	r = kill(my_pid, SIGTERM);
	if(r < 0)
		printf("kill failed: %s\n", strerror(errno));

	for(i = 0; i < 100; i++) {
		r = check_process_stop(my_pid);
		if(r == 0) {
			my_pid = 0;
			break;
		}
		usleep(100);
	}

	if(my_pid != 0) {
		printf("waitpid did not succeed: r = %d\n, pocess %d is now a zombie process", r, my_pid);
		my_pid = 0;
	}

	return 0;
}

// sysconf
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int main()
{
	int ncore;
	errno = 0;

	if ((ncore = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
		if (errno == 0)
			printf("_SC_NPROCESSORS_ONLN not supported by this implementation.\n");
		else
			perror("sysconf error.");
	else
		printf("ncore = %d\n", ncore);
}

// sysinfo
#include <sys/sysinfo.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define MB (1024 * 1024)

int main(void)
{
	int r;
	struct sysinfo si;
	int totalram, freeram, percent_inuse;

	r = sysinfo(&si);
	if(r < 0) {
		printf("sysinfo failed: %s", strerror(errno));
		return -1;
	}

	totalram = si.totalram/MB;
	freeram = si.freeram/MB;
	percent_inuse = (int)((float)(totalram - freeram) / (float)totalram * 100.0);

	printf("totalram: %d(MB), freeram:%d(MB), percent_inuse:%d\%\n",
		totalram, freeram, percent_inuse);

	return 0;
}

// readlink
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>

int main(void)
{
	int r;
	char *tz_path;
	char lpath[256], cmd[128];
	const char *symlink = "/etc/localtime";

	r = readlink(symlink, lpath, sizeof(lpath));
	if(r < 0) {
		printf("readlink failed\n");
		return -1;
	}

	printf("The value for symbol link %s is %s\n", symlink, lpath);

	tz_path = "/usr/share/zoneinfo/Asia/Shanghai";
	snprintf(cmd, sizeof(cmd), "ln -sf %s /etc/localtime", tz_path);

	r = system(cmd);
	if(r != 0) {
		printf("system failed\n");
		return -1;
	}
	printf("symlink %s to target %s succeed\n", symlink, tz_path);

	return 0;
}

// stat, fstat
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
	struct stat st;
	int r, n, fd;
	char *buf;

	fd = open("file.txt", O_RDONLY);
	if(fd < 0) {
		printf("open failed\n");
		return -1;
	}

	r = fstat(fd, &st);
	if(r < 0 || st.st_size == 0)
		n = 1024;
	else
		n = st.st_size;

	buf = malloc(n);

	n = read(fd, buf, n);
	if(n > 0)
		printf("buf:%s", buf);

	free(buf);
	close(fd);

	return 0;
}

// uname
#include <sys/types.h>
#include <sys/utsname.h>
#include <stdio.h>

int main()
{
	struct utsname sysInfo;
	if (uname(&sysInfo) != -1) {
		puts(sysInfo.sysname);
		puts(sysInfo.nodename);
		puts(sysInfo.release);
		puts(sysInfo.version);
		puts(sysInfo.machine);
	}
	else
		perror("uname() error");
}
```

[**Extensions to the C Language Family**](https://gcc.gnu.org/onlinedocs/gcc-12.2.0/gcc/C-Extensions.html)  
[GNU C Language Manual](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/index.html#SEC_Contents)  
[Online Python to C Converter](https://www.codeconvert.ai/python-to-c-converter)  
[The GNU C Reference Manual](https://www.gnu.org/software/gnu-c-manual/gnu-c-manual.html)  
[C Operator Precedence](https://en.cppreference.com/w/c/language/operator_precedence)  
[**The GNU C Library**](https://www.gnu.org/software/libc/manual/html_node/index.html#SEC_Contents) #online  
[**The GNU C Library**](http://herbert.the-little-red-haired-girl.org/html/glibc/libc_toc.html)  
[**The GNU C Library (glibc) manual**](https://sourceware.org/glibc/manual/)  
[c函数使用参考实例](https://bytefreaks.net/category/programming-2/c-programming-2)  
[POXIS Function Reference Example](https://support.sas.com/documentation/onlinedoc/sasc/doc/lr2/lrv2ch20.htm#lr2posix)  
[C standard](https://www.open-std.org/jtc1/sc22/wg14/www/standards.html)  
[**glibc source code**](https://elixir.bootlin.com/glibc/latest/source) #sourcecode  
[The GNU C Library](https://www.gnu.org/software/libc/manual/html_mono/libc.html)  
[Memory Layout Of A C Program](https://hackthedeveloper.com/memory-layout-c-program/)  
[C Program Compilation Process](https://hackthedeveloper.com/c-program-compilation-process/)  
[C 语言常见问题集](https://c-faq-chn.sourceforge.net/ccfaq/ccfaq.html)  
[Notes for C and C++ Programming](https://www.codeinsideout.com/blog/c-cpp/notes/)  
[c for dummies](https://c-for-dummies.com/)  
[C and C++ Projects](https://www.codewithc.com/c-projects-with-source-code/)  
[How to split/tokenize a string? (strtok() string tokenizer)](https://www.equestionanswers.com/c/c-strtok.php)  
[declaring a flexible array member](https://wiki.sei.cmu.edu/confluence/display/c/DCL38-C.+Use+the+correct+syntax+when+declaring+a+flexible+array+member)  
[How to interpret complex C/C++ declarations](https://www.codeproject.com/Articles/7042/How-to-interpret-complex-C-C-declarations)  
[Reading C type declarations](http://unixwiz.net/techtips/reading-cdecl.html)  
[cdecl.org](https://cdecl.org/)  

[Awesome C](https://github.com/oz123/awesome-c?tab=readme-ov-file#awesome-c) #github  
[C](https://github.com/marcotrosi/C)  
[**Programming in C**](https://users.cs.cf.ac.uk/dave/C/)  
[HOWTO: Use Address Sanitizer](https://www.osc.edu/resources/getting_started/howto/howto_use_address_sanitizer)  
[Clang 19.0.0git documentation](https://clang.llvm.org/docs/index.html)  
[Signal Handling](https://www.gnu.org/software/libc/manual/html_node/Signal-Handling.html)  
[Introduction To Unix Signals Programming](https://www.cs.kent.edu/~ruttan/sysprog/lectures/signals.html)  
[CS 43203 : System Programming](https://www.cs.kent.edu/~ruttan/sysprog/)  
[Unix Pthreads tutorial](https://www.cs.kent.edu/~ruttan/sysprog/)  
[C program to implement linked list](https://www.programmingsimplified.com/c/data-structures)  
[Using Templates and Generics in C](https://levelup.gitconnected.com/using-templates-and-generics-in-c-968da223154d)  
[Tutorial: Generics in C](https://itnext.io/tutorial-generics-in-c-b3362b3376a3)  
[Generic Programming in C](https://cs.boisestate.edu/~amit/teaching/253/handouts/07-c-generic-coding-handout.pdf)  
[void * and Function Pointers](https://web.stanford.edu/class/archive/cs/cs107/cs107.1202/lab4/)  
[Functional Pointer and Callback in C++](https://primerpy.medium.com/functional-pointer-and-callback-in-c-86c208df6b2f)  
[C and C++ Language Syntax Reference](https://www.cprogramming.com/reference/)  
[Understanding the LD_LIBRARY_PATH Environment Variable](https://tecadmin.net/understanding-the-ld_library_path-environment-variable/)  
[Arrays of Length Zero](https://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html)  
[Bounded Flexible Arrays in C](https://people.kernel.org/kees/bounded-flexible-arrays-in-c)  
[Flexible Array Members for C++](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1039r0.html)  
[Zero-length arrays in C have to go at the end of the struct](https://shivankaul.com/blog/zero-length-arrays)  
[How to Use typeof, Statement Expressions and Block-Scope Label Names](https://www.oracle.com/solaris/technologies/c-type.html)  
[The Unofficial C For Dummies Website](https://c-for-dummies.com/)  