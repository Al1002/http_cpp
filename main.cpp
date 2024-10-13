#include <stdlib.h>
#include <iostream> 
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <list>
/**
 * @brief An http/1.1 conditionally compliant server implementation
 * 
 */
class HTTPServer
{
    int serverSocket; // the listening socket file descriptor
    sockaddr_in serverAddress; // internet address object, default IPv4 0.0.0.0:8080
    std::list<int> clientSockets; // client file descriptors
public:
    /**
     * @brief Construct a new HTTPServer object.
     * 
     * @param port The port to listen to. If none is provided, assume 8080.
     */
    HTTPServer(int port = 8080)
    {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0); // TCP IP with "custom" protocol
    
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port);
        serverAddress.sin_addr.s_addr = INADDR_ANY;

        bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

        listen(serverSocket, 5);
    }

    /**
     * @brief Start handling of clients
     * 
     */
    void begin()
    {
        // poll(the listening socket + all the clients and parse the requests)
    }
};
int main(void)
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    
    listen(serverSocket, 5);
    int clientSocket = accept(serverSocket, nullptr, nullptr);
    std::cout<<"Wake-up recieved"<<'\n';
    sleep(1);
    while (true)
    {
        clientSocket = accept(serverSocket, nullptr, nullptr);
        char buffer[1024] = {0};
        recv(clientSocket, buffer, sizeof(buffer), 0);
        std::cout << "Message from client: " << buffer << std::endl;
    }
    
    select(1, clientSocket, nullptr, nullptr, timeval())
    close(serverSocket);
}
