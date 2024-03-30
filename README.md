A Chat Server using three different types of approach is implemented. The three approaches are
1. Iterative Server
2. Pre-Threading Server
3. Pre-Forking Server

**In Chat Server/server.cpp**

**Server Class**  

* An abstract base class Server is created with pure virtual functions startAcceptingClients, receiveMessages, handle_client, enter_server_loop, and setup_listening_socket.The implemenation is defined in the inherited classes.
* The abstract base class is inherited to implement the three different server approaches

**Iterative Class**  

* The Iterative class implements the iterative server approach.
* In Iterative Class a socket is created, binded to the specified port and enters a loop to start accepting and handling client connections.
* For each client accepted a thread is created for receiveMessages function. Since recv() is a blocking function it will not allow the multiple clients to be accepted in the server hence the receiveMessages function is running in a seperate thread allowing multiple clients to connect.

**PreThreading Class**  

* The PreThreading class implements the Pre-Threaded server approach.
* In this approach a pool of threads is created and each threads are used to handle the clients connected.
* In PreThreading Class a socket is created, binded to the specified port, a pool of threads is created and each thread enters a loop to start accepting and handling client connections.
* By using this pre-created pool of threads, the server can handle multiple client connections concurrently.

**PreForking Class**  

* The PreForking class implements the Pre-Fork server approach.
* It is similiar to prethreading but here a fixed number of child processes is created using fork() to handle clients.
* In PreForking Class a socket is created, binded to the specified port, creates a fixed number of child processes and each child process is responsible for accepting and handling client connections.
* Shared memory is used to share the messages among child processes so that all the clients connected to different processes can receive the message.
* When ever a process receives a message from a client the message along with client name is written in the shared memory and signal is sent to other processes so that other processes can read the message in the shared memory and send to their respective clients.
* Finally when an interrupt is received the all the child process is killed only after that the parent process is killed. The pids of the created child processes are stored in an array.

**LOAD TESTING**  

* Each Server is Load Tested individually in seperate folders.
  1. Load Testing Iterative Server
  2. Load Testing Pre-Threading Server
  3. Load Testing Pre-Forking Server
* In each folder a bash script to used to simulate the N number of clients.
* First the server is started then the N number of clients are created and connected to the server.
* To run N no of clients the bash script in the testing folder is runned as "./run_clients.sh <number_of_clients>".
* The performance and resource used is documented in a seperate file.
