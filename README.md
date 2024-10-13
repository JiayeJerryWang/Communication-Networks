
# Communication Networks

## Project Structure

- **HTTP Client and Server**
  - **Objective**: Implement a basic HTTP client and server.
  - **Client**: The client fetches files from any web server using the HTTP protocol.
  - **Server**: The server handles incoming HTTP GET requests and serves files to the client.
  - **Concurrency**: The server supports concurrent connections, meaning multiple clients can download files simultaneously without waiting for other connections to finish.

- **Reliable Data Transfer over UDP**
  - **Objective**: Implement a reliable data transfer protocol using UDP, simulating TCP.
  - **Sender**: Sends data packets over a lossy network, implementing retransmissions and congestion control.
  - **Receiver**: Receives the data packets, handles out-of-order packets, and writes the data to a file.
  - **TCP-Friendliness**: The protocol ensures fairness when competing with TCP traffic on the network.

- **Link State and Distance Vector Routing**
  - **Objective**: Implement two different routing algorithms: Link State and Distance Vector.
  - **Link State**: Nodes compute shortest paths based on global knowledge of the network.
  - **Distance Vector**: Nodes exchange information with their neighbors to compute routing tables.
  - **Topology Changes**: Both protocols adapt to dynamic changes in the network topology.

## Getting Started

### Prerequisites

- **C/C++**: The code is written in C, and you must have a working C compiler (like `gcc`).
- **Make**: The projects use `Makefile` for building the executables.
- **Git**: The project uses Git for version control.

### Setup

1. Clone the repository:

2. Compile the projects using `make`:
   ```bash
   make
   ```

3. Follow the specific instructions for each machine problem below.

---

## MP1: HTTP Client and Server

### Objective
In this MP, you will implement a simple HTTP client and server. The client fetches files from a web server, and the server serves files to clients.

### HTTP Client
- The client should be executed as:
  ```bash
  ./http_client http://hostname[:port]/path/to/file
  ```
- If no port is specified, use the default HTTP port (80). The client fetches the requested file and saves it as `output`.

### HTTP Server
- The server should be executed as:
  ```bash
  ./http_server <port>
  ```
- The server listens on the specified port and serves files from the current directory.

### Concurrency
- The server must handle multiple client requests concurrently without blocking.
- Use multi-threading or asynchronous I/O to achieve concurrency.

---

## MP2: Reliable Data Transfer over UDP

### Objective
In this MP, you will implement a reliable transport protocol over UDP, similar to TCP.

### Sender
- Use the function `reliablyTransfer()` to send files over the network:
  ```c
  void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
  ```
- The sender must handle packet loss, retransmissions, and congestion control.

### Receiver
- The function `reliablyReceive()` is responsible for receiving the data and saving it to a file:
  ```c
  void reliablyReceive(unsigned short int myUDPport, char* destinationFile);
  ```
- The receiver must handle out-of-order packets and acknowledge received data.

### Testing Environment
- Use two VirtualBox VMs to simulate the sender and receiver.
- Set up a lossy network environment using `tc`:
  ```bash
  sudo tc qdisc add dev eth0 root handle 1:0 netem delay 20ms loss 5%
  sudo tc qdisc add dev eth0 parent 1:1 handle 10: tbf rate 20Mbit burst 10mb latency 1ms
  ```

---

## MP3: Link State and Distance Vector Routing

### Objective
In this MP, you will implement both Link State and Distance Vector routing protocols.

### Link State Routing
- Implement a program called `linkstate` that simulates the Link State routing algorithm.
- The nodes exchange global network information and compute the shortest paths to all other nodes.

### Distance Vector Routing
- Implement a program called `distvec` that simulates the Distance Vector routing algorithm.
- The nodes exchange routing information with their neighbors and compute routing tables.

### Execution
- Both programs should be executed with the following command:
  ```bash
  ./linkstate <topologyfile> <messagefile> <changesfile>
  ./distvec <topologyfile> <messagefile> <changesfile>
  ```

### Output
- The output should include the routing tables for all nodes and the messages transmitted through the network.

---

