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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

// Global Variables and Structures
// si_other, s, slen: 
    // These are used for managing the UDP socket and the address of the receiver.

// RTT, MSS, data_length: 
    // Constants for setting the round-trip time, maximum segment size, and the amount of data per packet.

// packets, timestamps, acks: 
    // Dynamically allocated arrays to store packets, their send times, and their acknowledgment statuses.

// Helper Functions
    // diep: A simple function to print error messages and exit the program.
    // offset: Calculates the difference between two packet numbers, useful for managing the sliding window.

// Main Logic

// reliablyTransfer: 
    // The core function to send a file over UDP reliably.
    // Opens the file to be sent and calculates the number of packets needed based on the file size and the data length per packet.
    // Sets up the UDP socket, including setting a receive timeout to handle retransmissions.
    // Implements the sliding window protocol:
    // Fills the window with packets to be sent.
    // Sends packets within the window that haven't been sent yet or need retransmission.
    // Handles acknowledgments from the receiver to move the window forward.
    // Adjusts the window size based on network conditions, using mechanisms similar to TCP's congestion control (slow start and congestion avoidance).
    // Reallocates memory for the sliding window arrays as the window size changes.
    // Detects and handles three duplicate ACKs and timeouts to trigger retransmissions.
    // Continues the process until the entire file is sent and acknowledged.

// main: 
    // Parses command-line arguments for receiver details, file name, and the number of bytes to transfer. 
    // Initializes the sliding window arrays and calls reliablyTransfer to start the file transfer.

// Key Protocol Features
// Sliding Window Protocol: 
    // Ensures that multiple packets can be in-flight simultaneously, increasing the efficiency of the transfer. The window size adapts based on network conditions, similar to TCP's congestion control.
// Acknowledgments and Retransmissions: 
    // The sender expects acknowledgments for each packet. It uses timeouts and duplicate ACKs to detect lost packets and retransmits them as necessary.
// Dynamic Window Adjustment: 
    // The window size is adjusted in response to network congestion signals (e.g., duplicate ACKs, timeouts), aiming to maximize throughput while avoiding network congestion.

struct sockaddr_in si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}


int RTT = 33; // Timeout
int MSS = 1472; // 1472 = MTU, see MP2 handout

char* packets;
struct timeval* timestamps; // corresponding packet timestamps
int* acks; // corresponding acks / flags

int send_packet_base = 0; // starting packet number 
int next_packet_num = 0; // unsend starting packet number 
int new_packet_num = 0; // empty starting packet number

int data_length = 1472 - sizeof(int) - sizeof(int); // data length in one packet: 1472 - seq_num - len_header

int window_size = 64; // num of packets within the window
int ssthresh = 128;
int dup_acks = 0;

int offset(int next, int start){
    int packet_interval = next - start;
    return  packet_interval;
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    // https://stackoverflow.com/a/4182564/11886484 Timeout
    struct timeval timeout;      
    timeout.tv_sec = 0;
    timeout.tv_usec = RTT * 1000;
    if (setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0)
        fprintf(stderr, "setsockopt failed\n");

	/* Send data and receive acknowledgements on s*/
    int FINISH = 0;
    int last_packet_length = bytesToTransfer % (MSS - 2 * sizeof(int));
    int num_packets = bytesToTransfer / data_length;
    int full_packets = bytesToTransfer / data_length;
    if (last_packet_length != 0){
        num_packets += 1;
    }
    int transferred = 0;
// bytesToTransfer != seq
    while (1){
        // printf("total:%d,FINISH:%d,new_packet:%d,base:%d,window:%d, thres:%d\n",num_packets, FINISH,new_packet_num, send_packet_base,window_size, ssthresh );
        // fill data
        while(new_packet_num < (send_packet_base + window_size)){
            // finish stuff
            if (FINISH == 1){
                if ((num_packets + 1) <= new_packet_num){
                }else{
                    FINISH = 0;
                }
            }
// printf("transfered:%d, reminder:%d\n", transferred, bytesToTransfer-transferred);

            if (FINISH == 1) break;

            if (new_packet_num == num_packets){
                transferred = bytesToTransfer;
            } else if (new_packet_num == (num_packets -1)){
                transferred = bytesToTransfer - last_packet_length;
            } else{
                transferred = data_length * new_packet_num;
            }

            if (transferred == bytesToTransfer && (FINISH == 0)){
                char* start_location = packets + offset(new_packet_num, send_packet_base) * MSS;
                int packet_num = -1;
                int total_length = bytesToTransfer;
                memcpy(start_location, &packet_num,  sizeof(int));
                memcpy(start_location + sizeof(int), &total_length,  sizeof(int));
                FINISH = 1;
                new_packet_num += 1;
                break;
            }

            // seq_num at the beginning, len, then binary data
            int local_length = data_length;
            if((transferred + local_length) > bytesToTransfer){
                local_length = last_packet_length;
            }

            char* start_location = packets + offset(new_packet_num, send_packet_base) * MSS;

            int packet_num = new_packet_num;
            memcpy(start_location, &packet_num,  sizeof(int));
            memcpy(start_location + sizeof(int), &local_length,  sizeof(int));
            
            long int file_offset = new_packet_num * data_length;
            int f_rv = fseek(fp, file_offset, SEEK_SET);
// printf("fill:%d\n", new_packet_num);
            size_t read_len = fread(start_location + sizeof(int) + sizeof(int), local_length, 1,fp);

            // TODO: resize window, fseek to last packet
            transferred = file_offset + local_length;
            new_packet_num += 1;
        }



        // send data
        while(next_packet_num < new_packet_num){
            char* start_location = packets + offset(next_packet_num, send_packet_base) * MSS;
            ssize_t send_len = sendto(s, start_location, 1472, 0,(struct sockaddr*) &si_other, slen);
            gettimeofday(& (timestamps[offset(next_packet_num, send_packet_base)]), NULL);
// printf("send:%d\n", next_packet_num);
            next_packet_num += 1;
        }


        int increase = 0;
        // receive data
        // for (int i = 0; i < (next_packet_num - send_packet_base); i++){
        // while(1){
        for (int i = 0; i < (next_packet_num - send_packet_base) / 2; i++){
            int ack = -2;
            ssize_t recv_len = recvfrom(s,&ack,sizeof(int),0,(struct sockaddr*)&si_other,&slen);
            if ((send_packet_base <= ack) && (ack < next_packet_num)){
                if (acks[ack - send_packet_base] == 1) dup_acks++;
                acks[ack - send_packet_base] = 1;
                increase = 1;
            }
            if ((FINISH == 1) && (ack == -1)){
                acks[offset(num_packets, send_packet_base)] = 1;
            }
//    printf("ack_BREAK:%d,%d,%d,finished:%d\n", ack, new_packet_num, send_packet_base, FINISH);
            if (ack == -2) break;
    // printf("ack_NOT_break:%d,%d,%d\n", ack, new_packet_num, send_packet_base);
        }

        if (increase == 1 && (dup_acks < 3)){
            if (window_size < ssthresh){
                int diff = window_size;
                window_size = window_size * 2;
                packets = realloc(packets, sizeof(char) * MSS * window_size);
                timestamps = realloc(timestamps, sizeof(struct timeval) * window_size);
                acks = realloc(acks, sizeof(int) * window_size);
    // printf("window_Larger:%d\n", window_size);
                // clean buffer
                memset(packets + diff * MSS, 0, (size_t) diff * MSS);
                memset(timestamps + diff , 0, (size_t) diff * sizeof(struct timeval));
                memset(acks + diff, 0, (size_t) diff * sizeof(int));
            }else{
                int diff = 2;
                window_size = window_size + 2;
                packets = realloc(packets, sizeof(char) * MSS * window_size);
                timestamps = realloc(timestamps, sizeof(struct timeval) * window_size);
                acks = realloc(acks, sizeof(int) * window_size);
    // printf("window_Larger:%d\n", window_size);
                // clean buffer
                memset(packets + (window_size - diff) * MSS, 0, (size_t) diff * MSS);
                memset(timestamps + (window_size - diff) , 0, (size_t) diff * sizeof(struct timeval));
                memset(acks + (window_size - diff), 0, (size_t) diff * sizeof(int));
            }
        }

        if (dup_acks >= 3){
            dup_acks = 0;

            ssthresh = window_size / 2;
            window_size = window_size / 2;
// printf("window_Smaller:%d\n", window_size);
            packets = realloc(packets, sizeof(char) * MSS * window_size);
            timestamps = realloc(timestamps, sizeof(struct timeval) * window_size);
            acks = realloc(acks, sizeof(int) * window_size);

            if (next_packet_num > send_packet_base + window_size){
                next_packet_num = send_packet_base + window_size;
            }
            if (new_packet_num > send_packet_base + window_size){
                new_packet_num = send_packet_base + window_size;
            }

        }

        // check timeout
        // https://stackoverflow.com/a/27448980/11886484
        struct timeval t1;
        gettimeofday(&t1, NULL);
        // millisecond
        int timeout_flag = 0;

        for (int i = 0; i < (next_packet_num - send_packet_base); i++){
            if(acks[i] != 1){
                struct timeval t0 = timestamps[i];
                float diff = (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
                if (diff >= RTT) {
                    ssize_t send_len = sendto(s, packets + i * MSS, 1472, 0,(struct sockaddr*) &si_other, slen);
    // printf("timeout:%d,diff:%f, base:%d, max:%d\n", send_packet_base + i,diff, send_packet_base,  send_packet_base + (next_packet_num - send_packet_base) -1);
                    // reset timer + timeout
                    gettimeofday(& (timestamps[i]), NULL);
                    timeout_flag = 1;
                }
            }
        }

        if (timeout_flag == 1){
            ssthresh = window_size / 2;
            window_size = 2;
// printf("window_Smaller:%d\n", window_size);
            packets = realloc(packets, sizeof(char) * MSS * window_size);
            timestamps = realloc(timestamps, sizeof(struct timeval) * window_size);
            acks = realloc(acks, sizeof(int) * window_size);

            if (next_packet_num > send_packet_base + window_size){
                next_packet_num = send_packet_base + window_size;
            }
            if (new_packet_num > send_packet_base + window_size){
                new_packet_num = send_packet_base + window_size;
            }

        }

        // move window
        int first_unack = 0;
        for (int i = 0; i < (next_packet_num - send_packet_base); i++){
            if (acks[i] == 0){
                first_unack = i;
                break;
            }
            first_unack++;
        }
        if (first_unack != 0){
            // move forward
            memmove(packets, packets + first_unack * MSS, (size_t)  (window_size - first_unack) * MSS);
            memmove(timestamps, timestamps + first_unack, (size_t) (window_size - first_unack) * sizeof(struct timeval));
            memmove(acks, acks + first_unack , (size_t) (window_size - first_unack) * sizeof(int));
            
            // clean buffer
            memset(packets + (window_size - first_unack) * MSS, 0, (size_t) first_unack * MSS);
            memset(timestamps + (window_size - first_unack) , 0, (size_t) first_unack * sizeof(struct timeval));
            memset(acks + (window_size - first_unack), 0, (size_t) first_unack * sizeof(int));

            // update idx
            send_packet_base += first_unack;
// printf("move:%d\n", send_packet_base);
        }

        if((FINISH == 1) && num_packets == send_packet_base){
            break;
        }
    }

    close(fp);

    printf("Closing the socket\n");
    close(s);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    // dynamic allocation, initialize to 0
    packets = calloc(window_size, sizeof(char) * MSS);
    timestamps = calloc(window_size, sizeof(struct timeval));
    acks = calloc(window_size, sizeof(int));

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    free(packets);
    free(timestamps);
    free(acks);

    return (EXIT_SUCCESS);
}
