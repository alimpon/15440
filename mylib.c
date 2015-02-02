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
#include <errno.h>
#define MAXMSGLEN 100
#define MAXSIZE 256

int (*orig_close)(int fd);

char* client(char *msg, int msglen) {
    char *serverip;
	char *serverport;
	unsigned short port;
	char* buf = malloc(2*sizeof(int));
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

    //printf("Connecting to server\n");
	// actually connect to the server
	rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);
    //printf("Connected to server\n");
	// send message to server
    //printf("Sending to server\n");
    send(sockfd, msg, msglen, 0);	// send message; should check return value
    //printf("Sent to server\n");
	// get message back
    //printf("Waiting for server response\n");
    rv = recv(sockfd, buf, 2*sizeof(int), 0);	// get message
	if (rv<0) err(1,0);			// in case something went wrong
    //printf("Got server response\n");
    //printf("Closing socket\n");
	// close socket
	orig_close(sockfd);
    //printf("Closed socket\n");
	return buf;
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
	//f//printf(stderr, "mylib: open called for path %s\n", pathname);
    //4 for name, 4 for flags, 4 for mode, 4 for size string, x for pathname (+1 if  char)
    int size = (int)strlen(pathname);
    int msglen = 4*sizeof(int)+sizeof(mode_t)+size;
    char buffer[msglen];
    char* answer = malloc(2*sizeof(int));

    int function_id = 1;
    int offset=0;
    int bufsize = 3*sizeof(int)+sizeof(mode_t)+size;
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &flags, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &size, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &m, sizeof(mode_t));
    offset += sizeof(mode_t);
    memcpy(buffer+offset, pathname, size);
    offset += size;
    
    answer = client(buffer, msglen);

    int fd = *((int *) answer);
    int error = *((int *) answer+1);

    if(fd == -1){
        errno = error;
    }

    //printf("Open - fd: %d, error: %d\n",fd,error);
    return fd;
}

int close(int fd) {
    int msglen = 3*sizeof(int);
    char buffer[msglen];
    char* answer = malloc(2*sizeof(int));
    
    int function_id = 2;
    int offset = 0;
    int bufsize = 2*sizeof(int);
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &fd, sizeof(int));
    offset += sizeof(int);
    answer = client(buffer,msglen);
    int err = *((int *) answer);
    int error = *((int *) answer+1);
    if(err == -1){
        errno = error;
    }
    //printf("Close - error: %d, errno: %d\n",err,error);
    return err;
}

ssize_t (*orig_read)(int fd, void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count) {
    //printf("Entered read\n");
    int msglen = 3*sizeof(int)+sizeof(size_t)+count;
    char buffer[msglen];
    char* answer = malloc(sizeof(int)+sizeof(size_t));

    int function_id = 4;
    int offset=0;
    int bufsize = 2*sizeof(int)+sizeof(size_t)+count;
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &fd, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &count, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(buffer+offset, buf, count);
    offset += count;
    //printf("Calling read\n");
    answer = client(buffer, msglen);
    
    int error = *((int *) answer);
    ssize_t num_bytes;
    memcpy(&num_bytes,(char*)answer+sizeof(int),sizeof(size_t));
    
    if(num_bytes == -1){
        errno = error;
    }
    //printf("Write - num bytes: %d, errno: %d\n",num_bytes,error);
    return num_bytes;
}

ssize_t (*orig_write)(int fd, const void *buf, size_t count);

ssize_t write(int fd, const void *buf, size_t count){
    //printf("Entered write\n");
    int msglen = 3*sizeof(int)+sizeof(size_t)+count;
    char buffer[msglen];
    char* answer = malloc(sizeof(int)+sizeof(size_t));

    int function_id = 3;
    int offset=0;
    int bufsize = 2*sizeof(int)+sizeof(size_t)+count;
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &fd, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &count, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(buffer+offset, buf, count);
    offset += count;
    //printf("Calling write\n");
    answer = client(buffer, msglen);
    
    int error = *((int *) answer);
    ssize_t num_bytes;
    memcpy(&num_bytes,(char*)answer+sizeof(int),sizeof(size_t));
    
    if(num_bytes == -1){
        errno = error;
    }
    //printf("Write - num bytes: %d, errno: %d\n",num_bytes,error);
    return num_bytes;
}

off_t (*orig_lseek)(int fd, off_t offset, int whence);

off_t lseek(int fd, off_t offset, int whence){
    int msglen = 4*sizeof(int)+sizeof(off_t);
    char buffer[msglen];
    char* answer = malloc(sizeof(int)+sizeof(off_t));

    int function_id = 5;
    int change=0;
    int bufsize = 3*sizeof(int)+sizeof(off_t);
    memcpy(buffer, &bufsize, sizeof(int));
    change += sizeof(int);
    memcpy(buffer+change, &function_id, sizeof(int));
    change += sizeof(int);
    memcpy(buffer+change, &fd, sizeof(int));
    change += sizeof(int);
    memcpy(buffer+change, &whence, sizeof(int));
    change += sizeof(int);
    memcpy(buffer+change, &offset, sizeof(off_t));
    change += sizeof(off_t);
    //printf("Calling write\n");
    answer = client(buffer, msglen);
    
    int error = *((int *) answer);
    off_t result;
    memcpy(&result,(char*)answer+sizeof(int),sizeof(off_t));
    
    if(result == -1){
        errno = error;
    }
    //printf("Write - num bytes: %d, errno: %d\n",num_bytes,error);
    return result;
}

int (*orig_stat)(const char *path, struct stat *buf);

int stat(const char *path, struct stat *buf){
    //printf("Entered stat\n");
    int pathsize = (int)strlen(path);
    int statsize = sizeof(struct stat);
    int msglen = 4*sizeof(int)+pathsize+statsize;
    char buffer[msglen];
    char* answer = malloc(2*sizeof(int));

    int function_id = 6;
    int offset=0;
    int bufsize = 3*sizeof(int)+pathsize+statsize;
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &pathsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &statsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, path, pathsize);
    offset += pathsize;
    memcpy(buffer+offset, buf, statsize);
    offset += statsize;
    //printf("Calling write\n");
    answer = client(buffer, msglen);
    
    int error = *((int *) answer);
    int status = *((int *) answer+1);

    if(status == -1){
        errno = error;
    }
    //printf("Write - num bytes: %d, errno: %d\n",num_bytes,error);
    return status;
}

int (*orig_xstat)(int version, const char *path, struct stat *buf);

int __xstat(int version, const char *path, struct stat *buf){
	// we just print a message, then call through to the original open function (from libc)
	//f//printf(stderr, "mylib: stat called for path %s\n", path);
    int pathsize = (int)strlen(path);
    int statsize = sizeof(struct stat);
    int msglen = 5*sizeof(int)+pathsize+statsize;
    char buffer[msglen];
    char* answer = malloc(2*sizeof(int));

    int function_id = 7;
    int offset=0;
    int bufsize = 4*sizeof(int)+pathsize+statsize;
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &version, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &pathsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &statsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, path, pathsize);
    offset += pathsize;
    memcpy(buffer+offset, buf, statsize);
    offset += statsize;
    //printf("Calling write\n");
    answer = client(buffer, msglen);
    
    int error = *((int *) answer);
    int status = *((int *) answer+1);

    if(status == -1){
        errno = error;
    }
    //printf("Write - num bytes: %d, errno: %d\n",num_bytes,error);
    return status;
}

int (*orig_unlink)(const char *pathname);

int unlink(const char *pathname){
    int pathsize = (int)strlen(pathname);
    int msglen = 3*sizeof(int)+pathsize;
    char buffer[msglen];
    char* answer = malloc(2*sizeof(int));

    int function_id = 8;
    int offset=0;
    int bufsize = 2*sizeof(int)+pathsize;
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &pathsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, pathname, pathsize);
    offset += pathsize;
    //printf("Calling write\n");
    answer = client(buffer, msglen);
    
    int error = *((int *) answer);
    int status = *((int *) answer+1);

    if(status == -1){
        errno = error;
    }
    //printf("Write - num bytes: %d, errno: %d\n",num_bytes,error);
    return status;
}

ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbytes , off_t *basep);

ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep){
    int size = (int)strlen(buf);
    int msglen = 4*sizeof(int)+size+sizeof(size_t)+sizeof(off_t);
    char buffer[msglen];
    char* answer = malloc(sizeof(int)+sizeof(ssize_t));

    int function_id = 9;
    int offset=0;
    int bufsize = 3*sizeof(int)+size+sizeof(size_t)+sizeof(off_t);
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &fd, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &size, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &nbytes, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(buffer+offset, basep, sizeof(off_t));
    offset += sizeof(off_t);
    memcpy(buffer+offset, buf, size);
    offset += size;
    //printf("Calling write\n");
    answer = client(buffer, msglen);
    
    int error = *((int *) answer);
    ssize_t result;
    memcpy(&result,(char*)answer+sizeof(int),sizeof(ssize_t));
    
    if(result == -1){
        errno = error;
    }
    //printf("Write - num bytes: %d, errno: %d\n",num_bytes,error);
    return result;
}

struct dirtreenode* (*orig_getdirtree)( const char *path );

struct dirtreenode* getdirtree( const char *path ){
    client("getdirtree",10);
    return orig_getdirtree(path);
}

void (*orig_freedirtree)( struct dirtreenode* dt );

void freedirtree( struct dirtreenode* dt ){
    client("freedirtree",11);
    return orig_freedirtree(dt);
}

// This function is automatically called when program is started
void _init(void) {
	// set function pointer orig_open to point to the original open function
	orig_open = dlsym(RTLD_NEXT, "open");
    orig_close = dlsym(RTLD_NEXT, "close");
    orig_read = dlsym(RTLD_NEXT, "read");
    orig_write = dlsym(RTLD_NEXT, "write");
    orig_lseek = dlsym(RTLD_NEXT, "lseek");
    orig_stat = dlsym(RTLD_NEXT, "stat");
    orig_unlink = dlsym(RTLD_NEXT, "unlink");
    orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
    orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
    orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
    orig_xstat = dlsym(RTLD_NEXT, "__xstat");
    //f//printf(stderr, "Init mylib\n");
}


