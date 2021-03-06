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
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <dirtree.h>
#include <dirent.h>
#define LIMIT 10000


int sockfd;

struct dirtreenode* DFS(int sessfd)
{ 
    int namesize;
    recv(sessfd, &namesize, sizeof(int), 0);
    char* name = malloc(namesize); 
    recv(sessfd, name, namesize, 0);
    int num_subdirs;
    recv(sessfd, &num_subdirs, sizeof(int), 0);
    struct dirtreenode* dt = (struct dirtreenode*)malloc(sizeof(struct dirtreenode));
    dt->name = name;
    dt->num_subdirs = num_subdirs;
    if(num_subdirs>0){
        dt->subdirs = malloc(sizeof(struct dirtreenode)*num_subdirs);
        int i;
        for(i=0; i< num_subdirs; i++){
            struct dirtreenode* child = DFS(sessfd);
            dt->subdirs[i] = child;
        }   
    }else{
        dt->subdirs = NULL;
    }
    return dt;
}


int (*orig_close)(int fd);

void client(char *msg, int msglen, char *buf) {
	int rv, sv;
    int count=0;
    // send message to server
    while(count<msglen){ 
        sv=send(sockfd, msg+count, msglen-count, 0);
        if(sv<0){
            //printf("SEND ERROR\n");
            break;
        }
        count += sv;
    }
    	
    // get message back
    char recsize[sizeof(int)];
    recv(sockfd,&recsize,sizeof(int),0);     
    int retsize;
    memcpy(&retsize,recsize,sizeof(int));
    if(retsize>0){
        char trans[retsize];
        count=0;
        while (count<retsize){ 
            rv=recv(sockfd, trans, retsize, 0);
            memcpy(buf+count, &trans, rv);
            count += rv;
        }
        if (rv<0) err(1,0);			// in case something went wrong
    }
}


// The following line declares a function pointer with the same 
// prototype as the open function.  
int (*orig_open)(const char *pathname, int flags, ...);  
// mode_t mode is needed when flags includes O_CREAT


// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
    mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}

    int size = (int)strlen(pathname);
    int msglen = 4*sizeof(int)+sizeof(mode_t)+size;
    char buffer[msglen];
    char answer[2*sizeof(int)];

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
    
    client(buffer, msglen, answer);

    int fd = *((int *) answer);
    int error = *((int *) answer+1);

    if(fd == -1 || error!=0){
        errno = error;
        return -1;
    }

    return fd+LIMIT;
}

int close(int fd) {
    if(fd <= LIMIT){
        return orig_close(fd);
    }else{
        fd -= LIMIT;
    }
    int msglen = 3*sizeof(int);
    char buffer[msglen];
    char answer[2*sizeof(int)];
    
    int function_id = 2;
    int offset = 0;
    int bufsize = 2*sizeof(int);
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &fd, sizeof(int));
    offset += sizeof(int);
    client(buffer,msglen,answer);
    int err = *((int *) answer);
    int error = *((int *) answer+1);
    if(err == -1){
        errno = error;
    }
    return err;
}

ssize_t (*orig_read)(int fd, void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count) {
    if(fd <= LIMIT){
        return orig_read(fd,buf,count);
    }else{
        fd -= LIMIT;
    }
    int msglen = 3*sizeof(int)+sizeof(size_t);
    char buffer[msglen];
    char answer[sizeof(int)+sizeof(ssize_t)+count];

    int function_id = 4;
    int offset=0;
    int bufsize = 2*sizeof(int)+sizeof(size_t);
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &fd, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &count, sizeof(size_t));
    offset += sizeof(size_t);

    client(buffer, msglen, answer);
    
    int error = *((int *) answer);
    ssize_t num_bytes;
    memcpy(&num_bytes,(char*)answer+sizeof(int),sizeof(ssize_t));
    memcpy(buf,(char*)answer+sizeof(int)+sizeof(ssize_t),num_bytes); 

    if(error != 0){
        errno = error;
        return -1;
    }
    return num_bytes;
}

ssize_t (*orig_write)(int fd, const void *buf, size_t count);

ssize_t write(int fd, const void *buf, size_t count){
    if(fd <= LIMIT){
        return orig_write(fd,buf,count);
    }else{
        fd -= LIMIT;
    }
    int msglen = 3*sizeof(int)+sizeof(size_t)+count;
    char buffer[msglen];
    char answer[sizeof(int)+sizeof(ssize_t)];

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
    client(buffer, msglen,answer);
    
    int error = *((int *) answer);
    ssize_t num_bytes;
    memcpy(&num_bytes,(char*)answer+sizeof(int),sizeof(ssize_t));
    
    if(num_bytes == -1){
        errno = error;
    }
    return num_bytes;
}

off_t (*orig_lseek)(int fd, off_t offset, int whence);

off_t lseek(int fd, off_t offset, int whence){
    if(fd <= LIMIT){
        return orig_lseek(fd,offset,whence);
    }else{
        fd -= LIMIT;
    }
    int msglen = 4*sizeof(int)+sizeof(off_t);
    char buffer[msglen];
    char answer[sizeof(int)+sizeof(off_t)];

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
    client(buffer, msglen,answer);
    
    int error = *((int *) answer);
    off_t result;
    memcpy(&result,(char*)answer+sizeof(int),sizeof(off_t));
    
    if(result == -1){
        errno = error;
    }
    return result;
}

int (*orig_stat)(const char *path, struct stat *buf);

int stat(const char *path, struct stat *buf){
    int pathsize = (int)strlen(path);
    int msglen = 3*sizeof(int)+pathsize;
    char buffer[msglen];
    char answer[2*sizeof(int)+sizeof(struct stat)];

    int function_id = 6;
    int offset=0;
    int bufsize = 2*sizeof(int)+pathsize;
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &pathsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, path, pathsize);
    offset += pathsize;
    client(buffer, msglen, answer);
     
    int error = *((int *) answer);
    int status = *((int *) answer+1);
    memcpy(buf,(char*)answer+2*sizeof(int),sizeof(struct stat)); 
    if( status == -1){
        errno = error;
    }
    return status;
}

int (*orig_xstat)(int version, const char *path, struct stat *buf);

int __xstat(int version, const char *path, struct stat *buf){
    int pathsize = (int)strlen(path);
    int msglen = 4*sizeof(int)+pathsize;
    char buffer[msglen];
    char answer[2*sizeof(int)+sizeof(struct stat)];

    int function_id = 7;
    int offset=0;
    int bufsize = 3*sizeof(int)+pathsize;
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &version, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &pathsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, path, pathsize);
    offset += pathsize;
    client(buffer, msglen, answer);
     
    int error = *((int *) answer);
    int status = *((int *) answer+1);
    memcpy(buf,(char*)answer+2*sizeof(int),sizeof(struct stat)); 
    if( status == -1){
        errno = error;
    }
    return status;
}

int (*orig_unlink)(const char *pathname);

int unlink(const char *pathname){
    int pathsize = (int)strlen(pathname);
    int msglen = 3*sizeof(int)+pathsize;
    char buffer[msglen];
    char answer[2*sizeof(int)];

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
    client(buffer, msglen,answer);
    
    int error = *((int *) answer);
    int status = *((int *) answer+1);

    if(status == -1){
        errno = error;
    }
    return status;
}

ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbytes , off_t *basep);

ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep){
    if(fd <= LIMIT){
        return orig_getdirentries(fd,buf,nbytes,basep);
    }else{
        fd -= LIMIT;
    }
    int msglen = 3*sizeof(int)+sizeof(size_t)+sizeof(off_t);
    char buffer[msglen];
    char answer[sizeof(int)+sizeof(ssize_t)+nbytes];

    int function_id = 9;
    int offset=0;
    int bufsize = 2*sizeof(int)+sizeof(size_t)+sizeof(off_t);
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &fd, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &nbytes, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(buffer+offset, basep, sizeof(off_t));
    offset += sizeof(off_t);

    client(buffer, msglen,answer);
     
    int error = *((int *) answer);
    ssize_t num_bytes;
    memcpy(&num_bytes,(char*)answer+sizeof(int),sizeof(ssize_t));
    memcpy(buf,(char*)answer+sizeof(int)+sizeof(ssize_t),num_bytes); 
    basep += num_bytes;
    if(error != 0){
        errno = error;
        return -1;
    }
    return num_bytes;
}

struct dirtreenode* (*orig_getdirtree)( const char *path );

struct dirtreenode* getdirtree( const char *path ){
    int pathsize = (int)strlen(path);
    int msglen = 3*sizeof(int)+pathsize;
    char buffer[msglen];

    int function_id = 10;
    int offset=0;
    int bufsize = 2*sizeof(int)+pathsize;
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &pathsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, path, pathsize);
    offset += pathsize;
    
    send(sockfd, buffer, msglen, 0);	
	int error;
    recv(sockfd, &error,sizeof(int),0);
    if (error == -1){
        errno = error;
	    orig_close(sockfd);
        return NULL;
    }
    struct dirtreenode* root = DFS(sockfd); 	
   
    errno = error;
    return root;
}

void (*orig_freedirtree)( struct dirtreenode* dt );

void freedirtree( struct dirtreenode* dt ){
    orig_freedirtree(dt);
}

// This function is automatically called when program is started
void _init(void) {
	// set function pointers to point to the original function
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

    char *serverip;
	char *serverport;
	int rv;
    unsigned short port;
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
    int flag = 1;
	int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 
            (char *) &flag, sizeof(int));
    if (rv<0 || result<0) err(1,0);
}

void _fini(void){
	// close socketi
    int msglen = 2*sizeof(int);
    char buffer[msglen];

    int function_id = 69;
    int offset=0;
    int bufsize = sizeof(int);
    memcpy(buffer, &bufsize, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer+offset, &function_id, sizeof(int));
    offset += sizeof(int);
    
    send(sockfd, buffer, msglen, 0);
    orig_close(sockfd);
}
