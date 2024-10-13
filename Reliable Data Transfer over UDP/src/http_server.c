/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// #define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

#define PORTLENGTH 6 // 5 digits + 1 end zero
#define MAXBUFFERSIZE 2000 // buffer size
#define CODEOK "HTTP/1.1 200 OK" // response code for OK
#define CODENOTFOUND "HTTP/1.1 404 Not Found" // response code for Not Found
#define CODEOTHER "HTTP/1.1 400 Bad Request" // response code for Other Errors

#define NEWLINE "\r\n" // CRLF for newlines

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return
	&(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{	
	// From Beej's server.c
	if (argc != 2) {
	    fprintf(stderr,"Port Number not specified\n");
	    exit(1);
	}

	char* PORT = argv[1]; // port number, first parameter
	unsigned char buf[MAXBUFFERSIZE]; // buffer to store input/output
	
	int recieved_bytes = 0;

	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");
	// printf("server: got connection from %s\n", PORT);

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			//
			//
			//
			// start to handle request
			// receive the request and store it into buf
			if ((recieved_bytes = recv(new_fd, buf, MAXBUFFERSIZE-1, 0)) == -1) {
				perror("recv");
				exit(1);
			}

			// process to get the file path
			char *path = buf +4;
			char *end = strchr(buf+4,' ');

			// printf("Test End:%s\n", end);
			if (path == NULL || end == NULL) {
				perror("strstr");
				exit(1);
			}

			int path_length = end - path;
			if (*path == '/'){
				path += 1;
				path_length -= 1;
			}
			
			char path_buf[path_length+1]; // buffer to store path
			memcpy(path_buf, path, path_length);
			path_buf[path_length] = '\0';

			// check if exists
			FILE *fptr;
			fptr = fopen(path_buf,"rb");
			if(fptr == NULL){
				send(new_fd, CODENOTFOUND, 22, 0);
			}else{
				send(new_fd, CODEOK, 15, 0);
			}
			send(new_fd, NEWLINE, 2, 0);
			send(new_fd, NEWLINE, 2, 0);

			// send file back
			fseek(fptr, 0, SEEK_END);
			unsigned long len = (unsigned long)ftell(fptr);	
			fseek (fptr, 0, SEEK_SET);
			
			while (1) {
				if (len == 0){
					break;
				}

				if (len > (MAXBUFFERSIZE -1)){
					fread(buf,MAXBUFFERSIZE -1,1,fptr);
					buf[MAXBUFFERSIZE] = '\0';
					send(new_fd, buf, MAXBUFFERSIZE-1, 0);
					len -= MAXBUFFERSIZE -1;
				}else{
					fread(buf,len,1,fptr);
					buf[len] = '\0';
					send(new_fd, buf, len, 0);
					len = 0;
				}
			}
		
			fclose(fptr);
			close(new_fd);
			exit(0);
			//
			//
			//
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

