/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

// #define PORT "80" // the port client will be connecting to 

#define MAXDATASIZE 2000 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char* url = argv[1];

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// parse the url
	int url_len = strlen(url);
	for (int i = 0; i < strlen(url); i++) {
		if (*url == ':') {
			url_len -= 2;
			url += 1;
			break;
		} else {
			url_len-=1;
			url+=1;
		}
	}
	char parsed_url[strlen(url + 2) + 1];
	strncpy(parsed_url, url + 2, strlen(url + 2));
	parsed_url[strlen(url + 2)] = '\0';
	// printf("url:%s\n", parsed_url);

	// parse the host name
	int host_len = 0;
	for (int i = 0; i < url_len; i++) {
		if (parsed_url[i] == '/') {
			break;
		} else {
			host_len += 1;
		}
	}
	char host[host_len + 1];
	strncpy(host, parsed_url, host_len);
	host[host_len] = '\0';
	// printf("host:%s\n", host);

	// parse the path
	int path_len = url_len - host_len;
	char path[path_len + 1];
	strncpy(path, parsed_url + host_len, path_len);
	path[path_len] = '\0';
	// printf("path:%s\n", path);

	// parse the file name
	int filename_len = 0;
	for (int i = path_len; i > 0; i--) {
		if (path[i] == '/') {
			break;
		} else {
			filename_len++;
		}
	}
	char filename[path_len + 1];
	strncpy(filename, path, path_len);
	filename[path_len] = '\0';
	// printf("filename:%s\n", filename);

	// parse the port number
	int port_len = host_len;
	for (int i = 0; i < host_len; i++) {
		if (host[i] == ':') {
			break;
		} else {
			port_len -= 1;
		}
		// printf("port:%d:%c\n", i, host[i]);
	}
	int port_not_found = 0;
	if (port_len == 0) {
		port_not_found = 1;
		port_len = 2;
	}
	char port[port_len + 1];
	if (port_not_found == 1) {
		// ascii for 8 = 56
		// *port = 56;
		// // ascii for 0 = 48
		// *(port + 1) = 48;
		strncpy(port, "80", 2);
	} else {
		// printf("port:%s\n", host + host_len - port_len + 1);
		strncpy(port, host + host_len - port_len + 1, port_len);
	}
	port[port_len] = '\0';
	// printf("port:%s\n", parsed_url);
	// printf("host:%s\n", host);

	host_len = 0;
	if (port_not_found == 0){
		for (int i = 0; i < url_len; i++) {
			if (parsed_url[i] == ':') {
				break;
			} else {
				host_len += 1;
			}
		}
	}
	host[host_len] = '\0';
	// printf("host:%s\n", host);
	
	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	// printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure


	// parse the get request
	char* temp_1 = "GET ";
	char* temp_2 = " HTTP/1.1\r\n\r\n";
	char* temp_3 = "User-Agent: Wget/1.12(linux-gnu)\r\n";
	char* temp_4 = "Host: ";
	char* temp_5 = "\r\n";
	char* temp_6 = "Connetction: Keep-Alive\r\n\r\n";
	char *request = malloc(strlen(temp_1) + strlen(path) + strlen(temp_2) +
	strlen(temp_3) + strlen(temp_4) + strlen(host) + strlen(temp_5) + strlen(temp_6));
	// char *request = malloc(strlen(temp_1) + strlen(path) + strlen(temp_2) +1);
	sprintf(request, "%s%s%s%s%s%s%s%s", temp_1, path, temp_2, temp_3, temp_4, host, temp_5, temp_6);
	// sprintf(request, "%s%s%s", temp_1, path, temp_2);
	request[strlen(temp_1) + strlen(path) + strlen(temp_2) + strlen(temp_3) + strlen(temp_4) + strlen(host) + strlen(temp_5) + strlen(temp_6)] = '\0';
	// printf("%s", request);

	// send the get request
	send(sockfd, request, strlen(request), 0);

	// receive the response, write the file to a file called "output"
	int is_header = 1;
	FILE* fp; 
	fp = fopen("output", "wb");
	// printf("Test:%d\n", __LINE__);
	while (1) {
		// memset(buf, 0, MAXDATASIZE);
		if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
			perror("recv");
			exit(1);
		}
		buf[numbytes] = '\0';
		// printf("Test:%d\n", __LINE__);
		// printf("client: received '%s'\n",buf);
		// printf("Test:%d\n", __LINE__);
		// printf("%s\n", buf);
		if (numbytes > 0) {
			if (is_header == 1) {
				int ret = 0;
				if ((ret = strncmp(buf, "HTTP/1.1 200 OK", 15)) != 0){
					break;
				}

				is_header = 0;
				char* data_start = strstr(buf, "\r\n\r\n") + 4;
				// char* buffer_test[10];
				// buffer_test[9] = '\0';
				// strncpy(buffer_test, data_start,9);
				// printf("Test:%s\n", buffer_test);
				fwrite(data_start, 1, numbytes - (data_start - buf), fp);
			} else {
				fwrite(buf, 1, numbytes, fp);
			}
		} else if (numbytes == 0) {	
			break;
		}
	}

	fclose(fp);
	free(request);
	close(sockfd);

	return 0;
}

