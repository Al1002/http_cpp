#include <stdlib.h>
#include <iostream> 
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(void)
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    char message[16] = "Wake up\0";
    for(int i = 0; i<10; i++)
    {
        int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        sprintf(message, "msg %d\0", i);
        printf("%s\n", message);
        connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
        send(clientSocket, message, 16, 0);
        close(clientSocket);
    }
    close(clientSocket);
}