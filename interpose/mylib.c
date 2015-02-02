#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <err.h>

#define MAXMSGLEN 100

int (*orig_close)(int fd);

int client(char *msg) {
	char *serverip;
	char *serverport;
	unsigned short port;
	//char buf[MAXMSGLEN+1];
	int sockfd, rv;
	struct sockaddr_in srv;
	
	// Get environment variable indicating the ip address of the server
	serverip = getenv("server15440");
	if (serverip){
    }
	else {
		serverip = "127.0.0.1";
	}
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport){
    }
	else {
		serverport = "15440";
	}
	port = (unsigned short)atoi(serverport);
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to point to server
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = inet_addr(serverip);	// IP address of server
	srv.sin_port = htons(port);			// server port

	// actually connect to the server
	rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);
	
	// send message to server
	send(sockfd, msg, strlen(msg), 0);	// send message; should check return value
	
	// get message back
	/*rv = recv(sockfd, buf, MAXMSGLEN, 0);	// get message
	if (rv<0) err(1,0);			// in case something went wrong
	buf[rv]=0;				// null terminate string to print
	printf("client got message: %s\n", buf);
	*/
	// close socket
	orig_close(sockfd);

	return 0;
}


// The following line declares a function pointer with the same prototype as the open function.  
int (*orig_open)(const char *pathname, int flags, ...);  // mode_t mode is needed when flags includes O_CREAT

// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}
	// we just print a message, then call through to the original open function (from libc)
	//fprintf(stderr, "mylib: open called for path %s\n", pathname);
	client("open");
    return orig_open(pathname, flags, m);
}

int close(int fd) {
    client("close");
    return orig_close(fd);
}

ssize_t (*orig_read)(int fd, void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count) {
    client("read");
    return orig_read(fd, buf, count);
}

ssize_t (*orig_write)(int fd, const void *buf, size_t count);

ssize_t write(int fd, const void *buf, size_t count){
    client("write");
    return orig_write(fd,buf,count);
}

off_t (*orig_lseek)(int fd, off_t offset, int whence);

off_t lseek(int fd, off_t offset, int whence){
    client("lseek");
    return orig_lseek(fd,offset,whence);
}

int (*orig_stat)(const char *path, struct stat *buf);

int stat(const char *path, struct stat *buf){
	// we just print a message, then call through to the original open function (from libc)
	//fprintf(stderr, "mylib: stat called for path %s\n", path);
	client("stat");
    return orig_stat(path, buf);
}

int (*orig_unlink)(const char *pathname);

int unlink(const char *pathname){
	//fprintf(stderr, "mylib: stat called for path %s\n", pathname);
    client("unlink");
    return orig_unlink(pathname);
}

ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbytes , off_t *basep);

ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep){
    client("getdirentries");
    return orig_getdirentries(fd,buf,nbytes,basep);
}

struct dirtreenode* (*orig_getdirtree)( const char *path );

struct dirtreenode* getdirtree( const char *path ){
    client("getdirtree");
    return orig_getdirtree(path);
}

void (*orig_freedirtree)( struct dirtreenode* dt );

void freedirtree( struct dirtreenode* dt ){
    client("freedirtree");
    return orig_freedirtree(dt);
}

// This function is automatically called when program is started
void _init(void) {
	// set function pointer orig_open to point to the original open function
	orig_open = dlsym(RTLD_NEXT, "open");
    orig_close = dlsym(RTLD_NEXT, "close");
    orig_read = dlsym(RTLD_NEXT, "read");
    orig_write = dlsym(RTLD_NEXT, "write");
    orig_lseek = dlsym(RTLD_NEXT, "seek");
    orig_stat = dlsym(RTLD_NEXT, "stat");
    orig_unlink = dlsym(RTLD_NEXT, "unlink");
    orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
    orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
    orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
    //fprintf(stderr, "Init mylib\n");
}


