#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

// Global Variables and Structures: 
    // It defines structures for socket addresses (si_me for the receiver, si_other for the sender)
    // and global variables for socket descriptor s, socket address length slen, buffer management, and sliding window control.

// diep Function: A simple error handling function that prints the error message provided to it and terminates the program.

// Sliding Window Protocol Variables:
    // recv_base: The base index of the current window.
    // window_size: The size of the sliding window, controlling how many packets can be unacknowledged at any time.
    // MSS: Maximum Segment Size, derived from the MTU (Maximum Transmission Unit), indicating the size of the largest packet that can be sent.

// Buffer Management:
    // dataBuff: A buffer to temporarily store received data before writing it to the file.
    // corresponding_len: An array storing the lengths of the data segments in the buffer.
    // in_order_acks: An array indicating which packets within the window have been acknowledged in order.

// reliablyReceive Function:
    // Initializes the UDP socket and binds it to the specified port.
    // Enters a loop to receive data packets. For each packet, it extracts the sequence number and data length, and then decides how to handle the packet based on its sequence number and the current window state.
    // Acknowledges received packets by sending back their sequence numbers.
    // Manages the sliding window, writing in-order data to the file, and adjusting the window and buffer accordingly.
    // Handles the termination signal (a packet with a sequence number of -1) and closes the file and socket.

// Main Function:
    // Parses command-line arguments for the UDP port and the output file name.
    // Allocates memory for the data buffer and control arrays.
    // Calls reliablyReceive to start receiving data.
    // Frees allocated memory after receiving is complete.
    // Key Features of the Protocol:

// Sliding Window:
    // Ensures that only a fixed number of packets are sent without receiving an acknowledgment, controlling the flow of data and allowing for congestion control.
// Acknowledgments: 
    // Packets are acknowledged by their sequence numbers, allowing the sender to know which packets have been received successfully.
// Buffer Management: 
    // Temporarily stores packets that have been received out of order, ensuring that data can be written to the file in the correct order.
// Error Handling: 
    // Uses the diep function for basic error reporting and exits on critical errors, such as failure to create a socket or bind to a port.

struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

int recv_base = 0;
int window_size = 128;
int MSS = 1472; // 1472 = MTU, see MP2 handout

char* dataBuff;
int stored_length = 0;
int* corresponding_len;
int* in_order_acks; 

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");

    FILE *fp;
    fp = fopen(destinationFile, "wb");
	
    /* Now receive data and send acknowledgements */ 
    char recvBuff[1472];
    int seq, ack, length = 0;
    int total_length = -1;
    while (1){   
// printf("ACK:%d, BREAK,stored:%d, total:%d\n", ack, stored_length,total_length);
        if(total_length == stored_length) break;

        memset(recvBuff, 0, sizeof (recvBuff));
        ssize_t recv_len = recvfrom(s,recvBuff,1472,0,(struct sockaddr*)&si_other,&slen);

        // seq_num at the beginning, len, then binary data
        memcpy(&seq, recvBuff, sizeof(int));
        memcpy(&length, recvBuff + sizeof(int), sizeof(int));

        // TODO: handle closing
        // seq = -1 means finish from server with empty data
        // if((seq == -1) && (length == stored_length)) {
        if((seq == -1)) {
            total_length = length;
            int stop = -1;
            ssize_t send_len = sendto(s, &stop, sizeof(int), 0,(struct sockaddr*) &si_other, slen);
// printf("ACK:%d, base:%d, max:%d, stored:%d, total:%d\n", seq, recv_base, recv_base + window_size-1, stored_length,total_length);
            continue;
        } else if (recv_base > seq && seq != -1){
            ack = seq;
            ssize_t send_len = sendto(s, &ack, sizeof(int), 0,(struct sockaddr*) &si_other, slen);
// printf("ACK:%d, base:%d, max:%d\n", seq, recv_base, recv_base + window_size-1);
        }else if((recv_base <= seq) && (seq < (recv_base + window_size))){
            if (in_order_acks[seq - recv_base] == 1) continue;
            memcpy(dataBuff + (seq - recv_base) * (MSS - 2 * sizeof(int)), recvBuff + 2 * sizeof(int), length);

            in_order_acks[seq - recv_base] = 1;
            corresponding_len[seq - recv_base] = length;
// printf("ObtainLength:%d\n", length);
// printf("ACK:%d, base:%d, max:%d\n", seq, recv_base, recv_base + window_size-1);
            ack = seq;
            ssize_t send_len = sendto(s, &ack, sizeof(int), 0,(struct sockaddr*) &si_other, slen);
        }

        // move window forward
        int undeliverable_idx = 0;
        for(int i = 0; i < window_size; i++){
            if (in_order_acks[i] == 0){
                undeliverable_idx = i;
                break;
            }
            undeliverable_idx++;
        }
        for(int i = 0; i < undeliverable_idx; i++){
            int corr_length = corresponding_len[i];
            size_t write_len = fwrite(dataBuff + i * (MSS - 2 * sizeof(int)), corr_length,1,fp);
// printf("PRE_STORED:%d, WRITE:%d\n",stored_length, corr_length);
            stored_length += corr_length;
        }
        if (undeliverable_idx != 0){
            memmove(dataBuff, dataBuff + undeliverable_idx * (MSS - 2 * sizeof(int)), (size_t)(window_size - undeliverable_idx) * (MSS - 2 * sizeof(int)));
            memmove(in_order_acks, in_order_acks + undeliverable_idx, (window_size - undeliverable_idx) * sizeof(int));
            memmove(corresponding_len, corresponding_len + undeliverable_idx, (window_size - undeliverable_idx) * sizeof(int));

            memset(dataBuff + (window_size - undeliverable_idx) * (MSS - 2 * sizeof(int)), 0, (size_t) undeliverable_idx * (MSS - 2 * sizeof(int)));
            memset(in_order_acks + (window_size - undeliverable_idx) , 0, undeliverable_idx * sizeof(int));
            memset(corresponding_len + (window_size - undeliverable_idx), 0, undeliverable_idx * sizeof(int));

            recv_base += undeliverable_idx;
        }
    }
    close(fp);

    close(s);
	printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    dataBuff = calloc(window_size * (MSS - 2 * sizeof(int)), sizeof(char));
    in_order_acks = calloc(window_size, sizeof(int));
    corresponding_len = calloc(window_size, sizeof(int));

    reliablyReceive(udpPort, argv[2]);

    free(dataBuff);
    free(in_order_acks);
    free(corresponding_len);
}