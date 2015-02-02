#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <errno.h>
#define MAXMSGLEN 100

int main(int argc, char**argv) {
	//printf("printing server\n");
	char *serverport;
	unsigned short port;
	int sockfd, sessfd, rv;
	struct sockaddr_in srv, cli;
	socklen_t sa_size;
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) port = (unsigned short)atoi(serverport);
	else port=15440;
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to indicate server port
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = htonl(INADDR_ANY);	// don't care IP address
	srv.sin_port = htons(port);			// server port

	// bind to our port
	rv = bind(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);
	
	// start listening for connections
	rv = listen(sockfd, 5);
	if (rv<0) err(1,0);
	
	// main server loop, handle clients one at a time, quit after 10 clients
	//for( i=0; i<10; i++ ) {
	
    while(1){	
		// wait for next client, get session socket
		sa_size = sizeof(struct sockaddr_in);
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
		if (sessfd<0) err(1,0);
	    
		// get messages and send replies to this client, until it goes away
        char bufsize[sizeof(int)]; 
        while ( (rv=recv(sessfd, bufsize, 4, 0)) > 0) {
            //printf("RVsize is: %d\n",rv);
            if(rv>=4){
                break;
            }
        }
        //printf("Out of size loop\n");
        int size = *((int  *)bufsize); 
        //printf("bufsize is %d\n",size);
        char* msg = malloc(size);
        char buf[size];
        int count=0;
        while ( (rv=recv(sessfd, buf, size, 0)) > 0) {
            //printf("RV is: %d\n",rv);
            count += rv;
            memcpy(msg+count-rv, &buf, rv);
            if(count>=size){
                break;
            }
        }
        //4 for name, 4 for flags, 4 for mode, 4 for size string, x for pathname (+1 if NULL char)
        int type = *((int *) msg);
        //printf("Type is: %d\n", type);
        
        switch (type) {
            case 1:
                {
                    //printf("It is open.\n");
                    int flags = *((int *) msg+1);
                    int size = *((int *) msg+2);
                    mode_t mode;
                    memcpy(&mode,(char*)msg+3*sizeof(int),sizeof(mode_t));
                    
                    char path[size+1];
                    
                    memcpy(path,(char*)msg+3*sizeof(int)+sizeof(mode_t),size);
                    path[size] = '\0';

                    //printf("Flags is: %d\n",flags);
                   // printf("Mode is: %d\n",mode);
                    //printf("Size is: %d\n",size);
                    //printf("Path is: %s\n\n",path);

                    int fd = open(path, flags, mode);
                    int error = errno;
                    
                    //printf("Open done, fd is: %d, errno is: %d\n",fd,error);
                    
                    char answer[2*sizeof(int)];
                    memcpy(answer, &fd, sizeof(int));
                    memcpy(answer+sizeof(int), &error, sizeof(int));

                    send(sessfd, answer, 2*sizeof(int), 0);
                }
                break;
                
            case 2:
                {
                    //printf("It is close.\n");
                    int fd = *((int *) msg+1);
                    //printf("Closing fd: %d\n", fd);
                    int err = close(fd);
                    //printf("Closed\n");
                    int error = errno;
                    
                    //printf("Closed done, error is: %d, errno is: %d\n",err,error);
                    
                    char answer[2*sizeof(int)];
                    memcpy(answer, &err, sizeof(int));
                    memcpy(answer+sizeof(int), &error, sizeof(int));
                    send(sessfd, answer, 2*sizeof(int), 0);
                    break;
                }
            case 3:
                {
                    //printf("It is write.\n");
                    int fd = *((int *) msg+1);
                    size_t count;
                    memcpy(&count,(char*)msg+2*sizeof(int),sizeof(size_t));
                    
                    char* buffer = malloc(count);
                    
                    //includes null char since sent over null char
                    memcpy(buffer,(char*)msg+2*sizeof(int)+sizeof(size_t),count);
                    
                    //printf("fd is: %d\n",fd);
                    //printf("count is: %zu\n",count);
                    //printf("buffer is: %s\n\n",buffer);

                    ssize_t num_bytes = write(fd, buffer, count);
                    int error = errno;
                    
                    //printf("Write done, num_bytes is: %d, errno is: %d\n",num_bytes,error);
                    
                    char answer[sizeof(int)+sizeof(ssize_t)];
                    memcpy(answer, &error, sizeof(int));
                    memcpy(answer+sizeof(int), &num_bytes, sizeof(ssize_t));

                    send(sessfd, answer, sizeof(int)+sizeof(ssize_t), 0);
                    free(buffer);
                }
                break;

            case 4:
                {
                    //printf("It is read.\n");
                    int fd = *((int *) msg+1);
                    size_t count;
                    memcpy(&count,(char*)msg+2*sizeof(int),sizeof(size_t));
                    
                    char* buffer = malloc(count);
                    
                    //includes null char since sent over null char
                    memcpy(buffer,(char*)msg+2*sizeof(int)+sizeof(size_t),count);
                    
                    //printf("fd is: %d\n",fd);
                    //printf("count is: %zu\n",count);
                    //printf("buffer is: %s\n\n",buffer);

                    ssize_t num_bytes = read(fd, buffer, count);
                    int error = errno;
                    
                    //printf("Write done, num_bytes is: %d, errno is: %d\n",num_bytes,error);
                    
                    char answer[sizeof(int)+sizeof(ssize_t)];
                    memcpy(answer, &error, sizeof(int));
                    memcpy(answer+sizeof(int), &num_bytes, sizeof(ssize_t));

                    send(sessfd, answer, sizeof(int)+sizeof(ssize_t), 0);
                    free(buffer);
                }
                break;
            case 5:
                {
                    //printf("It is lseek.\n");
                    int fd = *((int *) msg+1);
                    int whence = *((int *) msg+2);
                    off_t offset;
                    
                    //includes null char since sent over null char
                    memcpy(&offset,(char*)msg+3*sizeof(int),sizeof(off_t));
                    
                    //printf("fd is: %d\n",fd);
                    //printf("count is: %zu\n",count);
                    //printf("buffer is: %s\n\n",buffer);

                    off_t result = lseek(fd, offset, whence);
                    int error = errno;
                    
                    //printf("Write done, num_bytes is: %d, errno is: %d\n",num_bytes,error);
                    
                    char answer[sizeof(int)+sizeof(off_t)];
                    memcpy(answer, &error, sizeof(int));
                    memcpy(answer+sizeof(int), &result, sizeof(off_t));

                    send(sessfd, answer, sizeof(int)+sizeof(off_t), 0);
                }
                break;
            case 6:
                {
                    //printf("It is stat.\n");
                    int pathsize = *((int *) msg+1);
                    int statsize = *((int *) msg+2);
                    char path[pathsize+1];
                    struct stat *buf = malloc(statsize);
                    memcpy(path,(char*)msg+3*sizeof(int), pathsize);
                    path[pathsize] = '\0';
                    memcpy(buf,(char*)msg+3*sizeof(int)+pathsize, statsize);

                    //printf("Flags is: %d\n",flags);
                    //printf("Mode is: %d\n",mode);
                    //printf("Size is: %d\n",size);
                    //printf("Path is: %s\n\n",path);

                    int result = stat(path, buf);
                    int error = errno;
                    
                    //printf("Open done, fd is: %d, errno is: %d\n",fd,error);
                    
                    char answer[2*sizeof(int)];
                    memcpy(answer, &error, sizeof(int));
                    memcpy(answer+sizeof(int), &result, sizeof(int));

                    send(sessfd, answer, 2*sizeof(int), 0);
                    free(buf);
                }
                break;
            case 7:
                {
                    //printf("It is xstat.\n");
                    int version = *((int *) msg+1);
                    int pathsize = *((int *) msg+2);
                    int statsize = *((int *) msg+3);
                    char path[pathsize+1];
                    struct stat *buf = malloc(statsize);
                    memcpy(path,(char*)msg+4*sizeof(int), pathsize);
                    path[pathsize] = '\0';
                    memcpy(buf,(char*)msg+4*sizeof(int)+pathsize, statsize);

                    //printf("Flags is: %d\n",flags);
                    //printf("Mode is: %d\n",mode);
                    //printf("Size is: %d\n",size);
                    //printf("Path is: %s\n\n",path);

                    int result = __xstat(version, path, buf);
                    int error = errno;
                    
                    //printf("Open done, fd is: %d, errno is: %d\n",fd,error);
                    
                    char answer[2*sizeof(int)];
                    memcpy(answer, &error, sizeof(int));
                    memcpy(answer+sizeof(int), &result, sizeof(int));

                    send(sessfd, answer, 2*sizeof(int), 0);
                    free(buf);
                }
                break;
            case 8:
                {
                    int pathsize = *((int *) msg+1);
                    char path[pathsize+1];
                    memcpy(path,(char*)msg+2*sizeof(int), pathsize);
                    path[pathsize] = '\0';

                    //printf("Flags is: %d\n",flags);
                    //printf("Mode is: %d\n",mode);
                    //printf("Size is: %d\n",size);
                    //printf("Path is: %s\n\n",path);

                    int result = unlink(path);
                    int error = errno;
                    
                    //printf("Open done, fd is: %d, errno is: %d\n",fd,error);
                    
                    char answer[2*sizeof(int)];
                    memcpy(answer, &error, sizeof(int));
                    memcpy(answer+sizeof(int), &result, sizeof(int));

                    send(sessfd, answer, 2*sizeof(int), 0);
                }
            case 9:
                {
                    int fd = *((int *) msg+1);
                    int size = *((int *) msg+2);
                    size_t nbytes;
                    memcpy(&nbytes,(char*)msg+3*sizeof(int), sizeof(size_t));
                    off_t *basep;
                    memcpy(basep,(char*)msg+3*sizeof(int)+sizeof(size_t), sizeof(off_t));
                    char buf[size+1];
                    memcpy(buf,(char*)msg+3*sizeof(int)+sizeof(size_t)+sizeof(off_t), size);
                    buf[size] = '\0';

                    //printf("Flags is: %d\n",flags);
                    //printf("Mode is: %d\n",mode);
                    //printf("Size is: %d\n",size);
                    //printf("Path is: %s\n\n",path);

                    ssize_t result = getdirentries(fd,buf,nbytes,basep);
                    int error = errno;
                    
                    //printf("Open done, fd is: %d, errno is: %d\n",fd,error);
                    
                    char answer[sizeof(int)+sizeof(ssize_t)];
                    memcpy(answer, &error, sizeof(int));
                    memcpy(answer+sizeof(int), &result, sizeof(ssize_t));

                    send(sessfd, answer, sizeof(int)+sizeof(ssize_t), 0);
                }
        }
        //printf("HI reached end\n");
        free(msg); 
		// either client closed connection, or error
		if (rv<0) err(1,0);
		close(sessfd);
	}
	
	// close socket
	close(sockfd);

	return 0;
}

