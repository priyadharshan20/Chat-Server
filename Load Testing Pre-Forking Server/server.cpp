#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <vector>
#include <thread>
#include <mutex>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/mman.h>

using namespace std;

#define PREFORK_CHILDREN 100
static pid_t pids[PREFORK_CHILDREN + 1];

mutex acceptlock;
mutex writeMutex;

int shmid = shmget((key_t)7654, sizeof((key_t)2030), 0666 | IPC_CREAT);

//shared memory to store resources used in different processes 
double *sharedMemforUserTime = (double *)mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 1, 0);
double *sharedMemforSystemTime = (double *)mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 2, 0);
double *sharedMemforRSS = (double *)mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 3, 0);
int *counter = (int *)mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 5, 0);

class Client
{
    int clientSocket = -1;
    char *username;
    int ratings;
    int client_id;

public:
    int getClientSocket()
    {
        return clientSocket;
    }

    char *getUsername()
    {
        return username;
    }

    void setClientSocket(int clientsocket)
    {
        this->clientSocket = clientsocket;
    }

    void setUsername(char *name)
    {
        this->username = new char(strlen(name) + 1);
        strcpy(this->username, name);
    }

    void setClientId(int id)
    {
        this->client_id = id;
    }

    int getClientId()
    {
        return client_id;
    }

    void setRating(int rating)
    {
        this->ratings = rating;
    }

    int getRating()
    {
        return ratings;
    }
};


class Server
{
public:
    virtual Client *startAcceptingClients(int serversocket) = 0;
    virtual void receiveMessages(Client *obj) = 0;
    virtual void handle_client(Client *obj) = 0;
    virtual void enter_server_loop(int server_socket) = 0;
    virtual int setup_listening_socket(int server_port) = 0;
};

class PreForking : public Server
{
public:
    static vector<int> connectedClients;
   

    static void send_message_signal(int signo)
    {

        writeMutex.lock();
        void *shared_memory = shmat(shmid, NULL, 0);
        for (int i = 0; i < PreForking::connectedClients.size(); i++)
        {
            send(PreForking::connectedClients[i], (char *)(shared_memory), strlen((char *)shared_memory), 0);
        }

        writeMutex.unlock();
    }

    Client *startAcceptingClients(int serversocket)
    {
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        int clientSocket = accept(serversocket, (struct sockaddr *)&clientAddress, &clientAddressLength);

        *counter += 1;
        char username[1024];
        ssize_t inputbytes = recv(clientSocket, (char *)&username, sizeof(username), 0);
        username[inputbytes] = '\0';

        Client *client = new Client();
        client->setClientSocket(clientSocket);
        client->setUsername(username);
        memset(username, '\0', sizeof(username));
        return client;
    };

    void receiveMessages(Client *obj)
    {
        char inputbuffer[1024];
        int clientsocket = obj->getClientSocket();

        char userName[1024];
        strcpy(userName, obj->getUsername());

        while (true)
        {
            ssize_t inputsize = recv(clientsocket, (char *)&inputbuffer, sizeof(inputbuffer), 0);

            if (inputsize > 0)
            {
                if (strcmp(inputbuffer, "exit") == 0)
                {

                    cout << "client breaked in process " << getpid() << "\n";
                    break;
                }

                inputbuffer[inputsize] = '\0';

                printf("%s: %s\n", obj->getUsername(), inputbuffer);

                writeMutex.lock();

                void *shared_memory = shmat(shmid, NULL, 0);
                string message = "";
                message += userName;
                message += " : ";
                message += inputbuffer;
                strcpy((char *)shared_memory, message.c_str());

                writeMutex.unlock();

                for (int i = 0; i < PREFORK_CHILDREN; i++)
                {
                    if (pids[i + 1] != getpid())
                    {
                        kill(pids[i + 1], SIGUSR1);
                    }
                }
            }

            memset(inputbuffer, '\0', sizeof(inputbuffer));
        }

        close(obj->getClientSocket());
        delete obj;
    };

    void handle_client(Client *obj)
    {

        thread receiveMessagesThread(&PreForking::receiveMessages, this, obj);
        receiveMessagesThread.detach();
    };

    void enter_server_loop(int server_socket)
    {
        while (1)
        {

            Client *client = startAcceptingClients(server_socket);

            cout << "ACCEPTED IN " << getpid() << endl;

            if (client->getClientSocket() == -1)
            {
                cout << "ERROR IN ACCEPTING THE SOCKET\n";
            }
            else
            {
                cout << "CLIENT " << client->getUsername() << " ACCEPTED SUCCESSFULLY fd = " << client->getClientSocket() << "\n";
            }

            handle_client(client);
        }
    };

    pid_t create_child(int index, int listening_socket)
    {
        pid_t pid;

        pid = fork();
        if (pid < 0)
        {
            cout << "fork() error\n";
        }
        else if (pid > 0)
        {
            return (pid);
        }

        printf("Server %d(pid: %ld) starting\n", index, (long)getpid());
        enter_server_loop(listening_socket);
    }

    int setup_listening_socket(int server_port)
    {

        signal(SIGUSR1, send_message_signal);

        int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        struct sockaddr_in serverAddress;
        memset(&serverAddress, 0, sizeof(serverAddress));

        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // host byte order to network byte order
        serverAddress.sin_port = htons(server_port);

        bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));

        if (listen(serverSocket, 10) == -1)
        {
            cout << "ERROR LISTERNING \n";
        }
        else
        {
            cout << "SERVER LISTENING ON PORT " << server_port << "\n";
        }

        for (int i = 0; i < PREFORK_CHILDREN; i++)
            pids[i + 1] = create_child(i, serverSocket);

        while (1)
            pause();
        return serverSocket;
    };
};

vector<int> PreForking::connectedClients;

void sigint_handler(int signo)
{
    pid_t pid;
    int status;

    if (getpid() != pids[0])
    {
        double user, sys;
        struct rusage myusage, childusage;

        getrusage(RUSAGE_SELF, &myusage);

        double current_resident_size = (double)myusage.ru_maxrss;
        *sharedMemforRSS += current_resident_size;

        user = (double)myusage.ru_utime.tv_sec + myusage.ru_utime.tv_usec / 1000000.0;
        sys = (double)myusage.ru_stime.tv_sec + myusage.ru_stime.tv_usec / 1000000.0;

        *sharedMemforUserTime += user;
        *sharedMemforSystemTime += sys;
        cout << "child process " << getpid() << " killed \n";
        kill(getpid(), SIGTERM);
        return;
    }

    while ((pid = waitpid(-1, &status, 0)) > 0)
        ;

    printf("Signal handler called From Parent Process\n");
    
    double user, sys;
    struct rusage myusage, childusage;

    getrusage(RUSAGE_SELF, &myusage);

    double current_resident_size = (double)myusage.ru_maxrss;
    *sharedMemforRSS += current_resident_size;

    user = (double)myusage.ru_utime.tv_sec + myusage.ru_utime.tv_usec / 1000000.0;
    sys = (double)myusage.ru_stime.tv_sec + myusage.ru_stime.tv_usec / 1000000.0;

    *sharedMemforUserTime += user;
    *sharedMemforSystemTime += sys;

    printf("\nNo. OF Clients : %d \n", *counter);
    printf("\nTime spent executing user instructions. (User time:) %lf seconds \n", *sharedMemforUserTime);
    printf("Time spent in operating system code on behalf of processes. (System time ) : %lf seconds \n", *sharedMemforSystemTime);
    printf("Maximum number of kilobytes of physical memory that processes used simultaneously (resident set size) : %lf kilobytes \n", *sharedMemforRSS);

    printf("\n stats is printed\n");
    exit(0);
}

int main()
{
    signal(SIGINT, sigint_handler);
    *sharedMemforUserTime = 0;
    *sharedMemforSystemTime = 0;
    *sharedMemforRSS = 0;
    *counter = 0;
    int serverSocket;
    int serverport = 52348;
    Server *server = new PreForking();
    pids[0] = getpid(); // parent process pid
    printf("PARENT PROCESS ID : %d\n", pids[0]);
    server->setup_listening_socket(serverport);
}