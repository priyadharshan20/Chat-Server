#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdlib>
#include <string.h>
#include <string>
#include <thread>
#include <vector>

using namespace std;

class Clientside
{
    char *userName;

public:
    void setUsername(char *name)
    {
        this->userName = new char(strlen(name) + 1);
        strcpy(this->userName, name);
    }

    char *getUsername()
    {
        return userName;
    }

    static void listenAndPrint(int clientSocket)
    {
        char inputbuffer[1024];
        while (true)
        {

            ssize_t inputsize = recv(clientSocket, (char *)&inputbuffer, sizeof(inputbuffer), 0);

            if (inputsize > 0)
            {
                inputbuffer[inputsize] = '\0';
                cout << "\n\n"
                     << inputbuffer << "\n\n";
            }
        }

        close(clientSocket);
    }

    void startListeningOnSeperateThread(int clientSocket)
    {
        thread listenThread(Clientside::listenAndPrint, clientSocket);
        listenThread.detach();
    }

    void sendUsername(int clientSocket)
    {

        setUsername(userName);
        ssize_t userNameFlag = send(clientSocket, (char *)getUsername(), strlen(userName), 0);
        memset(userName, '\0', sizeof(userName));
        if (userNameFlag >= 0)
        {
            cout << "\nUSERNAME SENT SUCCESSFULLY\n\n";
        }
        else
        {
            cout << "\nSENDING USERNAME FAILED\n\n";
        }
    }

    void connectToServer(int clientsocket)
    {
        struct sockaddr_in serverAddress;
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverAddress.sin_port = htons(52348);

        if (connect(clientsocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) != -1)
        {
            cout << "CONNECTED SUCCESSFULLY \n";
            sendUsername(clientsocket);
        }
        else
        {
            cout << "ERROR CONNECTION \n";
        }

        startListeningOnSeperateThread(clientsocket);
     
        while (true)
        {

            char outputbuffer[1024];
            cin.getline(outputbuffer, 1024);

            send(clientsocket, (char *)outputbuffer, strlen(outputbuffer), 0);
            if (strcmp(outputbuffer, "exit") == 0)
            {
                cout << "client exited\n";
                break;
            }

            memset(outputbuffer, '\0', sizeof(outputbuffer));
        }

        close(clientsocket);
    }

    void createSocket(char *username)
    {
        int clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (clientSocket == -1)
        {
            cout << "ERROR CREATING THE SOCKET \n";
        }
        else
        {
            cout << "CLIENT SOCKET " << clientSocket << " SUCCESSFULLY\n";
        }
        setUsername(username);
        connectToServer(clientSocket);
    }
};

int main(int argc,char *argv[])
{

    Clientside *client = new Clientside();
    client->createSocket(argv[1]);
}
