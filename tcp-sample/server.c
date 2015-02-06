#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirtree.h>
#include <dirent.h>
#define MAXMSGLEN 100

void DFS(struct dirtreenode* dt, int sessfd)
{
    int namesize = (int)strlen(dt->name)+1;
    send(sessfd,&namesize,sizeof(int),0);
    send(sessfd,dt->name,namesize,0);
    int num = dt->num_subdirs;
    send(sessfd,&num,sizeof(int),0);
    //printf("Num is: %d\n",dt->num_subdirs); 
    int i;
    for(i=0; i< dt->num_subdirs; i++){
        DFS(dt->subdirs[i],sessfd);
    }
    //printf("sent all children\n");
}

int main(int argc, char**argv) {
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
	int flag=1;
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    if (rv<0 || result<0) err(1,0);
	
	// start listening for connections
	rv = listen(sockfd, 5);
	if (rv<0) err(1,0);
	
	// main server loop, handle clients one at a time
    while(1){	
		// wait for next client, get session socket
		sa_size = sizeof(struct sockaddr_in);
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
		
        if (sessfd<0) err(1,0);
	    
        rv = fork();
        if(rv!=0) continue;
        while(1) {
            //printf("Entered loop\n");
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
            //printf("hiagain\n");
            char* msg = malloc(size);
            char* buf = malloc(size);
            //printf("hiagain2\n");
            int count=0;
            while ( (rv=recv(sessfd, buf, size, 0)) > 0) {
                //printf("RV is: %d\n",rv);
                memcpy(msg+count, buf, rv);
                count += rv;
                if(count>=size){
                    break;
                }
            }
            free(buf);
            //4 for name, 4 for flags, 4 for mode, 4 for size string, x for pathname (+1 if NULL char)
            int type = *((int *) msg);
            //printf("Type is: %d\n", type);
            
            switch (type) {
                case 1:
                    {
                        //fprintf(stderr,"It is open.\n");
                        int flags = *((int *) msg+1);
                        int size = *((int *) msg+2);
                        mode_t mode;
                        memcpy(&mode,(char*)msg+3*sizeof(int),sizeof(mode_t));
                        
                        char path[size+1];
                        
                        memcpy(path,(char*)msg+3*sizeof(int)+sizeof(mode_t),size);
                        path[size] = '\0';

                        //fprintf(stderr,"Flags is: %d\n",flags);
                        //fprintf(stderr,"Mode is: %d\n",mode);
                        //fprintf(stderr,"Size is: %d\n",size);
                        //fprintf(stderr,"Path is: %s\n\n",path);
                        
                        errno=0;
                        int fd = open(path, flags, mode);
                        int error = errno;
                        
                        //fprintf(stderr,"Open done, fd is: %d, errno is: %d\n",fd,error);
                        
                        char retsize[sizeof(int)];
                        size = 2*sizeof(int);
                        memcpy(retsize,&size,sizeof(int));
                        send(sessfd, retsize,sizeof(int),0);
                        
                        char answer[2*sizeof(int)];
                        memcpy(answer, &fd, sizeof(int));
                        memcpy(answer+sizeof(int), &error, sizeof(int));
                        send(sessfd, answer, 2*sizeof(int), 0);
                    } break;
                    
                case 2:
                    {
                        //fprintf(stderr,"It is close.\n");
                        int fd = *((int *) msg+1);
                        //fprintf(stderr,"Closing fd: %d\n", fd);
                        errno=0;
                        int err = close(fd);
                        int error = errno;
                        
                        //fprintf(stderr,"Closed done, error is: %d, errno is: %d\n",err,error);
                       
                        char retsize[sizeof(int)];
                        int size = 2*sizeof(int);
                        memcpy(retsize,&size,sizeof(int));
                        send(sessfd, retsize,sizeof(int),0);

                        char answer[2*sizeof(int)];
                        memcpy(answer, &err, sizeof(int));
                        memcpy(answer+sizeof(int), &error, sizeof(int));
                        send(sessfd, answer, 2*sizeof(int), 0);
                    } break;
     
                case 3:
                    {
                        //fprintf(stderr,"It is write.\n");
                        int fd = *((int *) msg+1);
                        size_t count;
                        memcpy(&count,(char*)msg+2*sizeof(int),sizeof(size_t));
                        
                        char* buffer = malloc(count);
                        
                        //includes null char since sent over null char
                        memcpy(buffer,(char*)msg+2*sizeof(int)+sizeof(size_t),count);
                        
                        //fprintf(stderr,"fd is: %d\n",fd);
                        //fprintf(stderr,"count is: %zu\n",count);
                        //fprintf(stderr,"buffer is: %s\n\n",buffer);
                        errno=0;
                        ssize_t num_bytes = write(fd, buffer, count);
                        int error = errno;
                        
                        //fprintf(stderr,"Write done, num_bytes is: %zd, errno is: %d\n",num_bytes,error);
                        
                        char retsize[sizeof(int)];
                        int size = sizeof(int)+sizeof(ssize_t);
                        memcpy(retsize,&size,sizeof(int));
                        send(sessfd, retsize,sizeof(int),0);
                        
                        char answer[sizeof(int)+sizeof(ssize_t)];
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer+sizeof(int), &num_bytes, sizeof(ssize_t));

                        send(sessfd, answer, sizeof(int)+sizeof(ssize_t), 0);
                        free(buffer);
                    } break;

                case 4:
                    {
                        //fprintf(stderr,"It is read.\n");
                        int fd = *((int *) msg+1);
                        size_t count;
                        memcpy(&count,(char*)msg+2*sizeof(int),sizeof(size_t));
                        
                        char buffer[count];
                        
                        //includes null char since sent over null char
                        
                        //fprintf(stderr,"fd is: %d\n",fd);
                        //printf("count is: %zu\n",count);
                        errno=0;
                        ssize_t num_bytes = read(fd, buffer, count);
                        int error = errno;
                        
                        if(num_bytes==-1){
                            num_bytes = 0;
                        }

                        //fprintf(stderr,"Read done, num_bytes is: %zd, errno is: %d\n",num_bytes,error);
                        //fprintf(stderr,"Buffer is: %s\n", buffer);
                        
                        char retsize[sizeof(int)];
                        int size = sizeof(int)+sizeof(ssize_t)+num_bytes;
                        memcpy(retsize,&size,sizeof(int));
                        send(sessfd, retsize,sizeof(int),0);
                        
                        char answer[sizeof(int)+sizeof(ssize_t)+num_bytes];
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer+sizeof(int), &num_bytes, sizeof(ssize_t));
                        memcpy(answer+sizeof(int)+sizeof(ssize_t), buffer, num_bytes);
                        send(sessfd, answer, sizeof(int)+sizeof(ssize_t)+num_bytes, 0);
                        //fprintf(stderr,"Supposed to send %zd\n",sizeof(int)+sizeof(ssize_t)+num_bytes);
                        //fprintf(stderr,"Actual sent %d\n",sent);
                    } break;
                case 5:
                    {
                        //printf("It is lseek.\n");
                        int fd = *((int *) msg+1);
                        int whence = *((int *) msg+2);
                        off_t offset;
                        
                        //includes null char since sent over null char
                        memcpy(&offset,(char*)msg+3*sizeof(int),sizeof(off_t));
                        
                        //printf("fd is: %d\n",fd);
                        errno=0;
                        off_t result = lseek(fd, offset, whence);
                        int error = errno;

                        char retsize[sizeof(int)];
                        int size = sizeof(int)+sizeof(off_t);
                        memcpy(retsize,&size,sizeof(int));
                        send(sessfd, retsize,sizeof(int),0);

                        char answer[sizeof(int)+sizeof(off_t)];
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer+sizeof(int), &result, sizeof(off_t));

                        send(sessfd, answer, sizeof(int)+sizeof(off_t), 0);
                    } break;
                case 6:
                    {
                        //printf("It is stat.\n");
                        int pathsize = *((int *) msg+1);
                        char path[pathsize+1];
                        memcpy(path,(char*)msg+2*sizeof(int), pathsize);
                        path[pathsize] = '\0';
                        struct stat *buf = malloc(sizeof(struct stat));
                        //printf("Path is: %s\n\n",path);
                        errno=0;
                        int result = stat(path, buf);
                        int error = errno;
                        
                        //printf("stat done, errno is: %d\n",error);

                        char retsize[sizeof(int)];
                        int size = 2*sizeof(int)+sizeof(struct stat);
                        memcpy(retsize,&size,sizeof(int));
                        send(sessfd, retsize,sizeof(int),0);

                        char answer[2*sizeof(int)+sizeof(struct stat)];
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer+sizeof(int), &result, sizeof(int));
                        memcpy(answer+2*sizeof(int), buf, sizeof(struct stat));
                        send(sessfd, answer, 2*sizeof(int)+sizeof(struct stat), 0);
                        free(buf);
                    } break;
                case 7:
                    {
                        //printf("It is xstat.\n");
                        int version = *((int *) msg+1);
                        int pathsize = *((int *) msg+2);
                        char path[pathsize+1];
                        memcpy(path,(char*)msg+3*sizeof(int), pathsize);
                        path[pathsize] = '\0';
                        struct stat *buf = malloc(sizeof(struct stat));
                        //printf("Version is: %d\n",version);
                        //printf("Size is: %d\n",pathsize);
                        //printf("Path is: %s\n\n",path);
                        errno=0;
                        int result = __xstat(version, path, buf);
                        int error = errno;
                         
                        //printf("xstat done, result is: %d, errno is: %d\n",result,error);

                        char retsize[sizeof(int)];
                        int size = 2*sizeof(int)+sizeof(struct stat);
                        memcpy(retsize,&size,sizeof(int));
                        send(sessfd, retsize,sizeof(int),0);

                        char answer[2*sizeof(int)+sizeof(struct stat)];
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer+sizeof(int), &result, sizeof(int));
                        memcpy(answer+2*sizeof(int), buf, sizeof(struct stat));
                        send(sessfd, answer, 2*sizeof(int)+sizeof(struct stat), 0);
                        free(buf);
                    } break;
                case 8:
                    {
                        //printf("It is unlink.\n");
                        int pathsize = *((int *) msg+1);
                        char path[pathsize+1];
                        memcpy(path,(char*)msg+2*sizeof(int), pathsize);
                        path[pathsize] = '\0';

                        //printf("Size is: %d\n",pathsize);
                        //printf("Path is: %s\n\n",path);
                        errno=0;
                        int result = unlink(path);
                        int error = errno;
                        
                        //printf("Unlink done, result is: %d, errno is: %d\n",result,error);

                        char retsize[sizeof(int)];
                        int size = 2*sizeof(int);
                        memcpy(retsize,&size,sizeof(int));
                        send(sessfd, retsize,sizeof(int),0);

                        char answer[2*sizeof(int)];
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer+sizeof(int), &result, sizeof(int));

                        //printf("Sending results back to client\n");

                        send(sessfd, answer, 2*sizeof(int), 0);
                    } break;
                case 9:
                    {
                        //printf("It is Getdirentries.\n");
                        int fd = *((int *) msg+1);
                        size_t nbytes;
                        memcpy(&nbytes,(char*)msg+2*sizeof(int), sizeof(size_t));
                        off_t basep;
                        memcpy(&basep,(char*)msg+2*sizeof(int)+sizeof(size_t), sizeof(off_t));
                        
                        char buf[nbytes];
                        errno=0;
                        ssize_t num_bytes = getdirentries(fd,buf,nbytes,&basep);
                        int error = errno;
                        
                        if(num_bytes==-1){
                            num_bytes=0;
                        }

                        printf("getdirentries done, num_bytes is: %zd, errno is: %d\n",num_bytes,error);
                         
                        char retsize[sizeof(int)];
                        int size = sizeof(int)+sizeof(ssize_t)+num_bytes;
                        memcpy(retsize,&size,sizeof(int));
                        send(sessfd, retsize,sizeof(int),0);

                        char answer[sizeof(int)+sizeof(ssize_t)+num_bytes];
                        memcpy(answer, &error, sizeof(int));
                        memcpy(answer+sizeof(int), &num_bytes, sizeof(ssize_t));
                        memcpy(answer+sizeof(int)+sizeof(ssize_t), buf, num_bytes);
                        send(sessfd, answer, sizeof(int)+sizeof(ssize_t)+num_bytes, 0);
                    } break;
                case 10:
                    {
                        //printf("It is getdirtree.\n");
                        int pathsize = *((int *) msg+1);
                        char path[pathsize+1];
                        memcpy(path,(char*)msg+2*sizeof(int), pathsize);
                        path[pathsize] = '\0';

                        //printf("Size is: %d\n",pathsize);
                        //printf("Path is: %s\n\n",path);
                        errno=0;
                        struct dirtreenode* tree = getdirtree(path);
                        int error = errno;
                        
                        //printf("getdirtree done, errno is: %d\n",error);
                        
                        //printf("Sending error back to client\n");
                        send(sessfd,&error,sizeof(int),0);
                        DFS(tree, sessfd);
                        freedirtree(tree);
                        //printf("Done with all DFS\n");
                    } break;
                default:
                    {
                      close(sessfd);
                      exit(0);
                    }
            }
            //printf("HI reached end\n");
            free(msg); 
            //printf("highfive\n");
            // either client closed connection, or error
            if (rv<0) err(1,0);
        }
    }
    // close socket
    close(sockfd);
	return 0;
}

