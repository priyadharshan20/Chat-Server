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

using namespace std;

// for threading
#define PRETHREADING_CHILDREN 10 

// for preforking
#define PREFORK_CHILDREN 10 
static pid_t pids[PREFORK_CHILDREN +1 ];

mutex acceptlock;
mutex writeMutex;
int shmid = shmget((key_t)7654, sizeof((key_t)2030), 0666 | IPC_CREAT);

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

// Client *client;

class Server
{
public:
    virtual Client *startAcceptingClients(int serversocket) = 0;
    virtual void receiveMessages(Client *obj) = 0;
    virtual void handle_client(Client *obj) = 0;
    virtual void enter_server_loop(int server_socket) = 0;
    virtual int setup_listening_socket(int server_port) = 0;
};

// ITERATIVE APPROACH
class Iterative : public Server
{
public:
    vector<Client *> ClientSockets;

    Client *startAcceptingClients(int serversocket)
    {
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        int clientSocket = accept(serversocket, (struct sockaddr *)&clientAddress, &clientAddressLength);

        char username[1024];
        ssize_t inputbytes = recv(clientSocket, (char *)&username, sizeof(username), 0);
        username[inputbytes] = '\0';

        Client *client = new Client();
        client->setClientSocket(clientSocket);
        client->setUsername(username);
        memset(username, '\0', sizeof(username));
        return client;
    }

    void receiveMessages(Client *obj)
    {

        char username[1024];
        strcpy(username, obj->getUsername());

        while (true)
        {
            char inputbuffer[1024];
            ssize_t inputbytesRead = recv(obj->getClientSocket(), inputbuffer, sizeof(inputbuffer), 0);
            // cout << "inputbytesRead " << inputbytesRead << "\n";

            if (inputbytesRead == -1)
            {
                cout << "ERROR IN RECEIVING MESSAGE";
                break;
            }
            else
            {
                if (strcmp(inputbuffer, "exit") == 0)
                {
                    cout << "CLIENT EXITED \n";
                    break;
                }
                else
                {
                    cout << username << " :  " << inputbuffer << "\n";
                    string message = "";
                    message += username;
                    message += " : ";
                    message += inputbuffer;

                    for (int i = 0; i < ClientSockets.size(); i++)
                    {
                        if (ClientSockets[i]->getClientSocket() != obj->getClientSocket())
                        {
                            send(ClientSockets[i]->getClientSocket(), (char *)message.c_str(), strlen(message.c_str()), 0);
                        }
                    }
                    memset(inputbuffer, '\0', sizeof(inputbuffer));
                }
            }
        }

        close(obj->getClientSocket());
        delete obj;
    }

    void handle_client(Client *obj)
    {
        cout << "client fd in handle client " << obj->getClientSocket() << "\n";
        thread threadformessages(&Iterative::receiveMessages, this, obj);
        threadformessages.detach();
    }

    void enter_server_loop(int server_socket)
    {
        while (true)
        {

            Client *client = startAcceptingClients(server_socket);

            ClientSockets.push_back(client);

            if (client->getClientSocket() == -1)
            {
                cout << "ERROR IN ACCEPTING THE SOCKET\n";
            }
            else
            {
                cout << "CLIENT " << client->getUsername() << " ACCEPTED SUCCESSFULLY\n";
            }

            handle_client(client);
        }
    }

    int setup_listening_socket(int server_port)
    {
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

        enter_server_loop(serverSocket);
        return (serverSocket);
    }
};

// THREADING APPROACH
class Threading : public Server
{
public:
    vector<thread> thread_pool;
    vector<Client *> ClientSockets;

    Client *startAcceptingClients(int serversocket)
    {
        acceptlock.lock();
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        int clientSocket = accept(serversocket, (struct sockaddr *)&clientAddress, &clientAddressLength);
        acceptlock.unlock();

        char username[1024];
        ssize_t inputbytes = recv(clientSocket, (char *)&username, sizeof(username), 0);
        username[inputbytes] = '\0';

        Client *client = new Client();
        client->setClientSocket(clientSocket);
        client->setUsername(username);
        memset(username, '\0', sizeof(username));
        return client;
    }

    void receiveMessages(Client *obj)
    {
        char username[1024];
        strcpy(username, obj->getUsername());

        while (true)
        {
            char inputbuffer[1024];
            ssize_t inputbytesRead = recv(obj->getClientSocket(), inputbuffer, sizeof(inputbuffer), 0);
            // cout << "inputbytesRead " << inputbytesRead << "\n";

            if (inputbytesRead == -1)
            {
                cout << "ERROR IN RECEIVING MESSAGE";
                break;
            }
            else
            {
                if (strcmp(inputbuffer, "exit") == 0)
                {
                    cout << "CLIENT EXITED \n";
                    break;
                }
                else
                {
                    cout << username << " :  " << inputbuffer << "\n";
                    string message = "";
                    message += username;
                    message += " : ";
                    message += inputbuffer;

                    for (int i = 0; i < ClientSockets.size(); i++)
                    {
                        if (ClientSockets[i]->getClientSocket() != obj->getClientSocket())
                        {
                            send(ClientSockets[i]->getClientSocket(), (char *)message.c_str(), strlen(message.c_str()), 0);
                        }
                    }
                    memset(inputbuffer, '\0', sizeof(inputbuffer));
                }
            }
        }

        close(obj->getClientSocket());
        delete obj;
    }

    void handle_client(Client *obj)
    {
        cout << "client fd in handle client " << obj->getClientSocket() << "\n";
        receiveMessages(obj);
    }

    void enter_server_loop(int server_socket)
    {
        while (true)
        {
            Client *client = startAcceptingClients(server_socket);

            ClientSockets.push_back(client);

            if (client->getClientSocket() == -1)
            {
                cout << "ERROR IN ACCEPTING THE SOCKET\n";
            }
            else
            {
                cout << "CLIENT " << client->getUsername() << " ACCEPTED SUCCESSFULLY\n";
            }

            handle_client(client);
        }
    }

    void create_thread(int serversocket)
    {
        thread_pool.push_back(thread(&Threading::enter_server_loop, this, serversocket));
    }

    int setup_listening_socket(int server_port)
    {
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

        for (int i = 0; i < PRETHREADING_CHILDREN; i++)
        {
            create_thread(serverSocket);
        }

        for (int i = 0; i < PRETHREADING_CHILDREN; i++)
        {
            thread_pool[i].join();
        }
        return (serverSocket);
    }
};

class PreForking : public Server
{
public:
    static vector<int> connectedClients;

    static void sigint_handler(int signo)
    {

        pid_t pid;
        int status;

        if (getpid() != pids[0])
        {
            cout << "child process " << getpid() << " killed \n";
            kill(getpid(), SIGTERM);
            return;
        }

        while ((pid = waitpid(-1, &status, 0)) > 0)
            ;

        printf("Signal handler called From Parent Process\n");

        exit(0);
    }

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

        // client_Socket = clientSocket;
        if (clientSocket != -1)
        {
            PreForking::connectedClients.push_back(clientSocket);
        }

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

                for (int i = 1; i <= PREFORK_CHILDREN; i++)
                {
                    if (pids[i] != getpid())
                    {
                        kill(pids[i], SIGUSR1);
                    }
                }
            }

            if (strcmp(inputbuffer, "exit") == 0)
            {

                cout << "client breaked in process " << getpid() << "\n";
                break;
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
        signal(SIGINT, sigint_handler);
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

        for (int i = 1; i <= PREFORK_CHILDREN; i++)
            pids[i] = create_child(i, serverSocket);

        while (1)
            pause();
        return serverSocket;
    };
};

vector<int> PreForking::connectedClients;

Server *createIterativeObject()
{
    Server *server = new Iterative();
    return server;
}

Server *createThreadingObject()
{
    Server *server = new Threading();
    return server;
}

Server *createForkingObject()
{
    Server *server = new PreForking();
    return server;
}

int main()
{

    int serverSocket;
    int serverport = 52345;
    Server *server;

    cout << "\tWelcome To The Server\t\n";
    cout << "\nPress 1 For Iterative Server\n";
    cout << "\nPress 2 For Threading Server\n";
    cout << "\nPress 3 For Forking Server\n";

    int choice;

temp:
    cout << "\nEnter Your Choice : ";
    cin >> choice;

    if (choice == 1)
    {
        server = createIterativeObject();
        server->setup_listening_socket(serverport);
    }
    else if (choice == 2)
    {
        server = createThreadingObject();
        server->setup_listening_socket(serverport);
    }
    else if (choice == 3)
    {
        server = createForkingObject();
        server->setup_listening_socket(serverport);
        pids[0] = getpid(); 
    }
    else
    {
        cout << "Enter a Valid Choice\n";
        goto temp;
    }

    close(serverSocket);
}
