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

mutex acceptlock;
#define NUMBEROFTHREADS 100

class Client
{
    int clientSocket;
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

// THREADING APPROACH
class Threading : public Server
{
public:
    vector<thread> thread_pool;
    vector<Client *> ClientSockets;
    static int counter;
    
    static void print_stats(int signo)
    {
        cout<<"\nsize of client socket vector : "<<Threading::counter<<"\n";

        double user, sys;
        struct rusage myusage;

        getrusage(RUSAGE_SELF, &myusage);

        double resident_size = (double)myusage.ru_maxrss;
    
        user = (double)myusage.ru_utime.tv_sec + myusage.ru_utime.tv_usec / 1000000.0;
        sys = (double)myusage.ru_stime.tv_sec + myusage.ru_stime.tv_usec / 1000000.0;

        
        printf("\nTime spent executing user instructions. (User time:) %lf seconds \n", user);
        printf("Time spent in operating system code on behalf of processes. (System time ) : %lf seconds \n",sys);
        printf("Maximum number of kilobytes of physical memory that processes used simultaneously (resident set size) : %lf kilobytes \n",resident_size);
        
        exit(0);
    }

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
            else if(inputbytesRead > 0)
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
        thread receiveMessagesThread(&Threading::receiveMessages,this,obj);
        receiveMessagesThread.detach();
    }

    void enter_server_loop(int server_socket)
    {
        while (true)
        {
            Client *client = startAcceptingClients(server_socket);

            ClientSockets.push_back(client);

            Threading::counter++;

            if (client->getClientSocket() == -1)
            {
                cout << "ERROR IN ACCEPTING THE SOCKET\n";
            }
            else
            {
                cout << "CLIENT " << client->getUsername() << " ACCEPTED SUCCESSFULLY\n";
            }
            char outputbuffer[1024] = "THREADING SERVER : ACKNOWLEDGEMENT";
            send(client->getClientSocket(), (char *)outputbuffer, strlen(outputbuffer), 0);
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

        signal(SIGINT,print_stats);

        struct sockaddr_in serverAddress;
        memset(&serverAddress, 0, sizeof(serverAddress));

        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
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

        for (int i = 0; i < NUMBEROFTHREADS; i++)
        {
            create_thread(serverSocket);
        }

        for (int i = 0; i < NUMBEROFTHREADS; i++)
        {
            thread_pool[i].join();
        }
        return (serverSocket);
    }
};

int Threading::counter = 0;

int main()
{
    int serverSocket;
    int serverport = 52347;
    Server *server = new Threading();
    server->setup_listening_socket(serverport);
}