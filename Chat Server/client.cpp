#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdlib>
#include <string.h>
#include <string>
#include <thread>

using namespace std;

class Clientside
{
    char *userName;

public:

    void setUsername(char *name){
        this->userName = new char(strlen(name)+1);
        strcpy(this->userName,name);
    }

    char* getUsername(){
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
                cout << "\n\n" << inputbuffer << "\n\n";
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

        char userName[1024];
        cout << "ENTER YOUR NAME : ";
        cin.getline(userName, 1024);
        setUsername(userName);
        ssize_t userNameFlag = send(clientSocket, (char *)&userName, strlen(userName), 0);
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

    void sendRatings(int clientSocket)
    {
        int rating;
    temp:
        cout << "RATE THE SESSION BETWEEN 0 TO 5 : ";
        cin >> rating;
        if (!(rating >= 0 && rating <= 5))
        {
            cout << "ENTER BETWEEN 0 TO 5";
            goto temp;
        }
        else
        {
            send(clientSocket, &rating, sizeof(rating), 0);
        }
    }

    void connectToServer(int clientsocket)
    {
        struct sockaddr_in serverAddress;
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
        serverAddress.sin_port = htons(52345);

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

            // cout << "MESSAGE SENT IS " << outputbuffer << "\n";
            send(clientsocket, (char *)outputbuffer, strlen(outputbuffer), 0);

            //to exit chat the user have to type and send "exit"
            
            if (strcmp(outputbuffer, "exit") == 0)
            {
                cout << "client exited\n";
                sendRatings(clientsocket);
                break;
            }

            memset(outputbuffer, '\0', sizeof(outputbuffer));
        }

        close(clientsocket);
    }

    void createSocket()
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

        connectToServer(clientSocket);
    }
};

int main()
{
    Clientside client;
    client.createSocket();
}
