- ```c
  int __argc;
  char **__argv;
  char*
  dpl_getarg(const char *var)
  {
  	int i, l;
  	char *p, *s;
  
  	for(i = 1; i < __argc; i++) {
  		s = __argv[i];
  		if(*s == '-')
  			s++;
  
  		p = s;
  		while(*p != '=' && *p != '\0')
  			p++;
  
  		if(*p != '=')
  			continue;
  
  		l = p - s;
  		if(var[l] == '\0' && memcmp(s, var, l) == 0)
  			return p+1;
  	}
  
  	return nil;
  }
  
  ioctl - control device
  manipulates the underlying device parameters of special files(e.g., character special files(terminals))
  
  	#include <sys/ioctl.h>
  
  	int ioctl(int fd, unsigned long request, ...);
  		fd: an open file
  		request: a device-dependent request code
  		third argument: an untyped pointer to memory (void *)
  
  
  fcntl - manipulate file descriptor
  performs operations determined by cmd on the open file descriptor fd
  
         #include <unistd.h>
         #include <fcntl.h>
  
         int fcntl(int fd, int cmd, ... /* arg */ );
  
  
  mmap, munmap - map or unmap files or devices into memory
  mmap()  creates  a  new mapping in the virtual address space of the calling process.  The starting address for the new mapping is specified in addr.  The length argument specifies the length of the mapping (which must be greater than 0).
  
         #include <sys/mman.h>
  
         void *mmap(void *addr, size_t length, int prot, int flags,
                    int fd, off_t offset);
         int munmap(void *addr, size_t length);
  
  fopen, fdopen, freopen - stream open functions
  The fopen() function opens the file whose name is the string pointed to by pathname and associates a stream with it.
  
  fread, fwrite - binary stream input/output
  The  function  fread() reads nmemb items of data, each size bytes long, from the stream pointed to by stream, storing them at the location given by ptr.
  The function fwrite() writes nmemb items of data, each size bytes long, to the stream  pointed  to by stream, obtaining them from the location given by ptr.
  
  fclose - close a stream
  clearerr, feof, ferror, fileno - check and reset stream status
  popen, pclose - pipe stream to or from a process
  
         #include <stdio.h>
  
         FILE *fopen(const char *pathname, const char *mode);
  
         FILE *fdopen(int fd, const char *mode);
  
         FILE *freopen(const char *pathname, const char *mode, FILE *stream);
  
         size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
  
         size_t fwrite(const void *ptr, size_t size, size_t nmemb,
                       FILE *stream);
  
         int fclose(FILE *stream);
  
         void clearerr(FILE *stream);
  
         int feof(FILE *stream);
  
         int ferror(FILE *stream);
  
         int fileno(FILE *stream);
  
         FILE *popen(const char *command, const char *type);
  
         int pclose(FILE *stream);
  
  
  open, openat, creat - open and possibly create a file
  
         #include <sys/types.h>
         #include <sys/stat.h>
         #include <fcntl.h>
  
         int open(const char *pathname, int flags);
         int open(const char *pathname, int flags, mode_t mode);
  
         int creat(const char *pathname, mode_t mode);
  
         int openat(int dirfd, const char *pathname, int flags);
         int openat(int dirfd, const char *pathname, int flags, mode_t mode);
  
  read - read from a file descriptor
  write - write to a file descriptor
  close - close a file descriptor
  
         #include <unistd.h>
  
         ssize_t read(int fd, void *buf, size_t count);
         ssize_t write(int fd, const void *buf, size_t count);
         int close(int fd);
  
  printf,  fprintf,  dprintf,  sprintf, snprintf, vprintf, vfprintf, vdprintf, vsprintf, vsnprintf - formatted output conversion
  
         #include <stdio.h>
  
         int printf(const char *format, ...);
         int fprintf(FILE *stream, const char *format, ...);
         int dprintf(int fd, const char *format, ...);
         int sprintf(char *str, const char *format, ...);
         int snprintf(char *str, size_t size, const char *format, ...);
  
         #include <stdarg.h>
  
         int vprintf(const char *format, va_list ap);
         int vfprintf(FILE *stream, const char *format, va_list ap);
         int vdprintf(int fd, const char *format, va_list ap);
         int vsprintf(char *str, const char *format, va_list ap);
         int vsnprintf(char *str, size_t size, const char *format, va_list ap);
  
  scanf, fscanf, sscanf, vscanf, vsscanf, vfscanf - input format conversion
  
         #include <stdio.h>
  
         int scanf(const char *format, ...);
         int fscanf(FILE *stream, const char *format, ...);
         int sscanf(const char *str, const char *format, ...);
  
         #include <stdarg.h>
  
         int vscanf(const char *format, va_list ap);
         int vsscanf(const char *str, const char *format, va_list ap);
         int vfscanf(FILE *stream, const char *format, va_list ap);
  
  strchr, strrchr, strchrnul - locate character in string
  
         #include <string.h>
  
         char *strchr(const char *s, int c);
  
         char *strrchr(const char *s, int c);
  
         #define _GNU_SOURCE         /* See feature_test_macros(7) */
         #include <string.h>
  
         char *strchrnul(const char *s, int c);
  
  strstr, strcasestr - locate a substring
  
         #include <string.h>
  
         char *strstr(const char *haystack, const char *needle);
  
         #define _GNU_SOURCE         /* See feature_test_macros(7) */
  
         #include <string.h>
  
         char *strcasestr(const char *haystack, const char *needle);
  
  strtok, strtok_r - extract tokens from strings
  The  strtok()  function  uses  a static buffer while parsing, so it's not thread safe.  Use strtok_r() if this matters to you
  
         #include <string.h>
  
         char *strtok(char *str, const char *delim);
  
         char *strtok_r(char *str, const char *delim, char **saveptr);
  
  strdup, strndup, strdupa, strndupa - duplicate a string
  strlen - calculate the length of a string
  strcmp, strncmp - compare two strings
  strcasecmp, strncasecmp - compare two strings ignoring case
  
         #include <string.h>
  
         char *strdup(const char *s);
  
         char *strndup(const char *s, size_t n);
         char *strdupa(const char *s);
         char *strndupa(const char *s, size_t n);
  
         size_t strlen(const char *s);
  
         int strcmp(const char *s1, const char *s2);
  
         int strncmp(const char *s1, const char *s2, size_t n);
  
         int strcasecmp(const char *s1, const char *s2);
  
         int strncasecmp(const char *s1, const char *s2, size_t n);
  
  
  access, faccessat - check user's permissions for a file
  
         #include <unistd.h>
  
         int access(const char *pathname, int mode);
  
         #include <fcntl.h>           /* Definition of AT_* constants */
         #include <unistd.h>
  
         int faccessat(int dirfd, const char *pathname, int mode, int flags);
  
  fgetc, fgets, getc, getchar, ungetc - input of characters and strings
  
         #include <stdio.h>
  
         int fgetc(FILE *stream);
  
         char *fgets(char *s, int size, FILE *stream);
  
         int getc(FILE *stream);
  
         int getchar(void);
  
         int ungetc(int c, FILE *stream);
  
  fgetc, fgets, getc, getchar, ungetc - input of characters and strings
  
         #include <stdio.h>
  
         int fgetc(FILE *stream);
  
         char *fgets(char *s, int size, FILE *stream);
  
         int getc(FILE *stream);
  
         int getchar(void);
  
         int ungetc(int c, FILE *stream);
  
  system - execute a shell command
  
         #include <stdlib.h>
  
         int system(const char *command);
  
  isalnum, isalpha, isascii, ... - character classification functions
  
         #include <ctype.h>
  
         int isalnum(int c);
         int isalpha(int c);
         int iscntrl(int c);
         int isdigit(int c);
         int isgraph(int c);
         int islower(int c);
         int isprint(int c);
         int ispunct(int c);
         int isspace(int c);
         int isupper(int c);
         int isxdigit(int c);
  
         int isascii(int c);
         int isblank(int c);
  
         int isalnum_l(int c, locale_t locale);
         int isalpha_l(int c, locale_t locale);
         int isblank_l(int c, locale_t locale);
         int iscntrl_l(int c, locale_t locale);
         int isdigit_l(int c, locale_t locale);
         int isgraph_l(int c, locale_t locale);
         int islower_l(int c, locale_t locale);
         int isprint_l(int c, locale_t locale);
         int ispunct_l(int c, locale_t locale);
         int isspace_l(int c, locale_t locale);
         int isupper_l(int c, locale_t locale);
         int isxdigit_l(int c, locale_t locale);
  
         int isascii_l(int c, locale_t locale);
  
  atoi, atol, atoll - convert a string to an integer
  
         #include <stdlib.h>
  
         int atoi(const char *nptr);
         long atol(const char *nptr);
         long long atoll(const char *nptr);
  
  strtoul, strtoull, strtouq - convert a string to an unsigned long integer
  
         #include <stdlib.h>
  
         unsigned long int strtoul(const char *nptr, char **endptr, int base);
  
         unsigned long long int strtoull(const char *nptr, char **endptr,
                                         int base);
         long int strtol(const char *nptr, char **endptr, int base);
  
         long long int strtoll(const char *nptr, char **endptr, int base);
  
  toupper, tolower, toupper_l, tolower_l - convert uppercase or lowercase
  
         #include <ctype.h>
  
         int toupper(int c);
         int tolower(int c);
  
         int toupper_l(int c, locale_t locale);
         int tolower_l(int c, locale_t locale);
  
  malloc, free, calloc, realloc - allocate and free dynamic memory
  
         #include <stdlib.h>
  
         void *malloc(size_t size);
         void free(void *ptr);
         void *calloc(size_t nmemb, size_t size);
         void *realloc(void *ptr, size_t size);
         void *reallocarray(void *ptr, size_t nmemb, size_t size);
  
  getenv, secure_getenv - get an environment variable
  
         #include <stdlib.h>
  
         char *getenv(const char *name);
  
         char *secure_getenv(const char *name);
  
  regcomp, regexec, regerror, regfree - POSIX regex functions
  
         #include <sys/types.h>
         #include <regex.h>
  
         int regcomp(regex_t *preg, const char *regex, int cflags);
  
         int regexec(const regex_t *preg, const char *string, size_t nmatch,
                     regmatch_t pmatch[], int eflags);
  
         size_t regerror(int errcode, const regex_t *preg, char *errbuf,
                         size_t errbuf_size);
  
         void regfree(regex_t *preg);
  
  fork - create a child process
  
         #include <sys/types.h>
         #include <unistd.h>
  
         pid_t fork(void);
  
  execl, execlp, execle, execv, execvp, execvpe - execute a file
  
         #include <unistd.h>
  
         extern char **environ;
  
         int execl(const char *path, const char *arg, ...
                         /* (char  *) NULL */);
         int execlp(const char *file, const char *arg, ...
                         /* (char  *) NULL */);
         int execle(const char *path, const char *arg, ...
                         /*, (char *) NULL, char * const envp[] */);
         int execv(const char *path, char *const argv[]);
         int execvp(const char *file, char *const argv[]);
         int execvpe(const char *file, char *const argv[],
                         char *const envp[]);
  
  
  wait, waitpid, waitid - wait for process to change state
  
         #include <sys/types.h>
         #include <sys/wait.h>
  
         pid_t wait(int *wstatus);
  
         pid_t waitpid(pid_t pid, int *wstatus, int options);
  
         int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options);
                         /* This is the glibc and POSIX interface; see
                            NOTES for information on the raw system call. */
  
  memset - fill memory with a constant byte
  memmove - copy memory area
  
         #include <string.h>
  
         void *memset(void *s, int c, size_t n);
         void *memmove(void *dest, const void *src, size_t n);
  
  kill - send signal to a process
  
         #include <sys/types.h>
         #include <signal.h>
  
         int kill(pid_t pid, int sig);
  
  reboot - reboot or enable/disable Ctrl-Alt-Del
  
         #include <unistd.h>
         #include <linux/reboot.h>
  
         int reboot(int magic, int magic2, int cmd, void *arg);
  
         #include <unistd.h>
         #include <sys/reboot.h>
  
         int reboot(int cmd);
  
  select, pselect, FD_CLR, FD_ISSET, FD_SET, FD_ZERO - synchronous I/O multiplexing
  
         /* According to POSIX.1-2001, POSIX.1-2008 */
         #include <sys/select.h>
  
         /* According to earlier standards */
         #include <sys/time.h>
         #include <sys/types.h>
         #include <unistd.h>
  
         int select(int nfds, fd_set *readfds, fd_set *writefds,
                    fd_set *exceptfds, struct timeval *timeout);
  
         void FD_CLR(int fd, fd_set *set);
         int  FD_ISSET(int fd, fd_set *set);
         void FD_SET(int fd, fd_set *set);
         void FD_ZERO(fd_set *set);
  
         #include <sys/select.h>
  
         int pselect(int nfds, fd_set *readfds, fd_set *writefds,
                     fd_set *exceptfds, const struct timespec *timeout,
                     const sigset_t *sigmask);
  
  poll, ppoll - wait for some event on a file descriptor
  
         #include <poll.h>
  
         int poll(struct pollfd *fds, nfds_t nfds, int timeout);
  
         #define _GNU_SOURCE         /* See feature_test_macros(7) */
         #include <signal.h>
         #include <poll.h>
  
         int ppoll(struct pollfd *fds, nfds_t nfds,
                 const struct timespec *tmo_p, const sigset_t *sigmask);
  
  epoll - I/O event notification facility (man 7 epoll)
  
         #include <sys/epoll.h>
  
  
  socket - create an endpoint for communication
  
         #include <sys/types.h>          /* See NOTES */
         #include <sys/socket.h>
  
         int socket(int domain, int type, int protocol);
  
  getsockopt, setsockopt - get and set options on sockets
  
         #include <sys/types.h>          /* See NOTES */
         #include <sys/socket.h>
  
         int getsockopt(int sockfd, int level, int optname,
                        void *optval, socklen_t *optlen);
         int setsockopt(int sockfd, int level, int optname,
                        const void *optval, socklen_t optlen);
  
  pthread_mutex_init — destroy and initialize a mutex
  pthread_mutex_lock, pthread_mutex_trylock, pthread_mutex_unlock — lock and unlock a mutex
  
         #include <pthread.h>
  
         int pthread_mutex_destroy(pthread_mutex_t *mutex);
         int pthread_mutex_init(pthread_mutex_t *restrict mutex,
             const pthread_mutexattr_t *restrict attr);
         pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  
         int pthread_mutex_lock(pthread_mutex_t *mutex);
         int pthread_mutex_trylock(pthread_mutex_t *mutex);
         int pthread_mutex_unlock(pthread_mutex_t *mutex);
  
  pthread_mutexattr_init, pthread_mutexattr_destroy - initialize and destroy a mutex attributes object
  
         #include <pthread.h>
  
         int pthread_mutexattr_init(pthread_mutexattr_t *attr);
         int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
  
         Compile and link with -pthread.
  
  
  shm_open, shm_unlink - create/open or unlink POSIX shared memory objects
  
         #include <sys/mman.h>
         #include <sys/stat.h>        /* For mode constants */
         #include <fcntl.h>           /* For O_* constants */
  
         int shm_open(const char *name, int oflag, mode_t mode);
  
         int shm_unlink(const char *name);
  
         Link with -lrt.
  
  truncate, ftruncate - truncate a file to a specified length
  
         #include <unistd.h>
         #include <sys/types.h>
  
         int truncate(const char *path, off_t length);
         int ftruncate(int fd, off_t length);
  
  
  pthread_self - obtain ID of the calling thread
  
         #include <pthread.h>
  
         pthread_t pthread_self(void);
  
         Compile and link with -pthread.
  
     
  pthread_setaffinity_np, pthread_getaffinity_np - set/get CPU affinity of a thread
  
         #define _GNU_SOURCE             /* See feature_test_macros(7) */
         #include <pthread.h>
  
         int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize,
                                    const cpu_set_t *cpuset);
         int pthread_getaffinity_np(pthread_t thread, size_t cpusetsize,
                                    cpu_set_t *cpuset);
  
         Compile and link with -pthread.
  
  
  strerror, strerror_r, strerror_l - return string describing error number
  
         #include <string.h>
  
         char *strerror(int errnum);
  
         int strerror_r(int errnum, char *buf, size_t buflen);
                     /* XSI-compliant */
  
         char *strerror_r(int errnum, char *buf, size_t buflen);
                     /* GNU-specific */
  
         char *strerror_l(int errnum, locale_t locale);
  ```