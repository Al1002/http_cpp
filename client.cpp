/**#include <stdlib.h>
#include <iostream> 
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(void)
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = INADDR_ANY;

    connect(clientSocket, (struct sockaddr*)&server_address, sizeof(server_address));

    // magic
    
    close(clientSocket);
}*/

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>  // For memset and strcpy
#include <string>

int main(void) {
    // Create the socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Error creating socket!" << std::endl;
        return 1;
    }

    // Define the server address
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");  // localhost

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        std::cerr << "Connection failed!" << std::endl;
        close(clientSocket);
        return 1;
    }
    std::cout<<"Connected!"<<'\n';
    // Prepare the HTTP request (minimal valid HTTP/1.1 request)
    std::string httpRequest = "GET / HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Connection: close\r\n\r\n";

    // Send the HTTP request to the server
    ssize_t sentBytes = send(clientSocket, httpRequest.c_str(), httpRequest.length(), 0);
    if (sentBytes < 0) {
        std::cerr << "Failed to send the request!" << std::endl;
        close(clientSocket);
        return 1;
    }

    // Buffer to store the response
    const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);  // Clear the buffer

    // Receive the server's response
    ssize_t receivedBytes;
    std::string serverResponse;
    
    while ((receivedBytes = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[receivedBytes] = '\0';  // Null-terminate the response
        serverResponse += buffer;      // Append the response to the string
        memset(buffer, 0, BUFFER_SIZE);  // Clear the buffer
    }

    // Check if the reception was successful
    if (receivedBytes < 0) {
        std::cerr << "Error receiving response!" << std::endl;
    } else {
        // Print the server's response
        std::cout << "Server Response:\n" << serverResponse << std::endl;
    }

    // Close the socket
    close(clientSocket);

    return 0;
}
