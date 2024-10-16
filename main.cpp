#include <stdlib.h>
#include <iostream> // cin/cout 
#include <unistd.h> // sleep
#include <sys/socket.h> // s.e.
#include <netinet/in.h> // bind/listen
#include <list> // s.e.
#include <poll.h> // async listening
#include <string> // s.e.
#include <map> // hash table
#include <unordered_map> 
#include <regex> // s.e.
#include <thread> // s.e.
#include <mutex> // wrong usage

using std::string;
using std::string;

class Request
{
public:
    enum Method{INVALID, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH};
    /**
     * @brief Parsing status
     * VALID - the request string parsed successfuly, the request can be handled safely.
     * INCOMPLETE - the request line/headers are incomplete. Re-run the request with parse() and more data.
     * BODY_INCOMPLETE - the request line/headers are complete. Re-run the request with parse_body() only.
     * MALFORMED - the string and request are malformed and should be discarded.
     * UNSUPORTED - the parser does not recognize the requested http version.
     */
    enum Status{INCOMPLETE, VALID, BODY_INCOMPLETE, MALFORMED, UNSUPORTED, TOO_LARGE};
    static const int MAX_REQUEST_SIZE = 16 * 1024;

    int response_fd = 0;
    string text;
    Method method;
    string uri;
    string version;
    std::map<string, string> headers;
    std::map<string, string> querry_params;
    int body_len = 0;
    char *body = nullptr;

    Request(){}

    static Method stringToMethod(string method_str)
    {
        if(method_str == "GET")
            return GET;
        if(method_str == "HEAD")
            return HEAD;
        if(method_str == "POST")
            return POST;
        if(method_str == "PUT")
            return PUT;
        if(method_str == "DELETE")
            return DELETE;
        if(method_str == "CONNECT")
            return CONNECT;
        if(method_str == "OPTIONS")
            return OPTIONS;
        if(method_str == "TRACE")
            return TRACE;
        if(method_str == "PATCH")
            return PATCH;
        return INVALID;
    };

    static Status parse_request_line(string& text, Request& out)
    {
        std::regex request_line_rgx("^(.*) ((?:\\/.*)+) (HTTP\\/1\\.1)\r\n");
        std::smatch match;
        if(!std::regex_search(text, match, request_line_rgx))
            return MALFORMED;
        string method_str = match[1].str();
        out.method = stringToMethod(method_str);
        if(out.method == INVALID)
            return MALFORMED;
        out.uri = match[2].str();
        out.version = match[3].str(); // TODO: this is dogshit, it doesnt eat the string and it doesnt validate it. use getline/split
        if(out.version != "HTTP/1.1")
            return UNSUPORTED;
        out.text += text.substr(0, text.find("\r\n") + 2);
        text.erase(0, text.find("\r\n") + 2);
        return VALID;
    }

    static Status parse_headers(string& text, Request& out)
    {
        std::regex headers_rgx("^([A-Za-z0-9-]+): *([\x09\x20-\x7E\x80-\xFF]+) *\r\n");
        std::smatch match;
        while(std::regex_search(text, match, headers_rgx))
        {
            if(out.headers.find(match[1]) != out.headers.end())
                out.headers[match[1]] = out.headers[match[1]] + " ," + match[2].str();
            else
                out.headers[match[1]] = match[2];
            out.text += text.substr(0, text.find("\r\n") + 2);
            text.erase(0, text.find("\r\n") + 2);
        }
        if(text.substr(0, 2) != "\r\n")
        {
            return MALFORMED;
        }
        out.text += text.substr(0, 2);
        text.erase(0, 2);
        return VALID;
    }

    static Status parse_body(string& text, Request& out)
    {
        if(out.headers.find("Content-Lenght") != out.headers.end())
        {
            out.body_len = std::stoi(out.headers["Content-Lenght"]);
            if(out.text.length() + out.body_len > MAX_REQUEST_SIZE)
                return TOO_LARGE;
            out.body = new char[out.body_len];
            if(text.length() < out.body_len)
                return BODY_INCOMPLETE;
            memccpy(out.body, text.c_str(), 1, out.body_len);
            text.erase(0, out.body_len);
        }
        else
        {
            out.body_len = 0;
            out.body = nullptr;
        }
        return VALID;
    }

    /**
     * @brief Populates request by consuming string. Returns status code for the resulting request.
     * 
     * @param response_fd 
     * @param text 
     * @param out 
     * @return Status  
     */
    static Status parse(int response_fd, string& text, Request& out)
    {
        out = Request();
        int heading_end = text.find("\r\n\r\n") + 3;
        if(heading_end == text.npos)
            return INCOMPLETE;
        if(heading_end > MAX_REQUEST_SIZE)
            return TOO_LARGE;
        out.response_fd = response_fd;
        Status status;
        status = parse_request_line(text, out);
        if(status != VALID)
            return status;
        status = parse_headers(text, out);
        if(status != VALID)
            return status;
        status = parse_body(text, out);
        if(status != VALID)
            return status;
        return VALID;
    }

    ~Request()
    {
        if(body != nullptr)
            delete body;
    }
};

class Client
{
public:
    int socket_fd;
    string msg;
    timeval last_seen;
    Request* current_request;
    Request::Status current_status;

    Client(int socket_fd)
    {
        this->socket_fd = socket_fd;
        current_request = nullptr;
        current_status = Request::Status::INCOMPLETE;
    }

    Request* extractRequest()
    {
        auto hold = current_request;
        current_request = nullptr;
        current_status = Request::Status::INCOMPLETE;
        return hold;
    }

    /**
     * @brief Attempts to update the request with new data, based on the status.
     * 
     * @return Request::Status 
     */
    Request::Status update()
    {
        static char data[4096];
        int data_len = recv(socket_fd, data, 4096 - 1, 0);
        if(data_len == -1)
            throw std::exception();
        data[data_len] = '\0';
        msg += data;
        if(current_request == nullptr)
        {
            current_request = new Request;
            current_status = Request::Status::INCOMPLETE;
        }
        if(current_status == Request::Status::BODY_INCOMPLETE)
            return Request::parse_body(msg, *current_request);
        return Request::parse(socket_fd, msg, *current_request);
    }
};

class Response
{
public:
    int response_fd;
    string version;
    int status_code;
    string reason;
    std::map<string, string> headers;
    int body_len;
    char *body;
    Response(int response_fd, string version, int status_code, string reason)
    {
        this->response_fd = response_fd;
        this->version = version;
        this->status_code = status_code;
        this->reason = reason;
    }
    static Response tooLarge(int response_fd)
    {
        return Response(response_fd, "HTTP/1.1", 413, "Payload Too Large");
    }
    static Response badRequest(int response_fd)
    {
        return Response(response_fd, "HTTP/1.1", 400, "Bad Request");
    }
    static Response requestTimeout(int response_fd)
    {
        return Response(response_fd, "HTTP/1.1", 408, "Request Timeout");
    }
    string toString()
    {
        string responce = version + " " + std::to_string(status_code) + " " + reason + "\r\n";
        for(auto iter = headers.begin(); iter != headers.end(); ++iter)
        {
            responce += iter->first + ": " + iter->second + "\r\n";
        }
        responce += "\r\n";
        return responce;
    }
    void respond()
    {
        string text = toString();
        std::cout << text;
        send(response_fd, text.c_str(), text.length(), 0);
    }
};

/**
 * @brief An http/1.1 conditionally compliant server implementation
 * 
 */
class HTTPServer
{
    void loop_wrap()
    {
        while(running)
            loop();
    }
public:
    static const short poll_events = POLLIN | POLLPRI | POLLHUP;   // polling event mask
                                                            //pollin = ready to read; anything else = the socket should be closed
    int server_socket; // the listening socket file descriptor
    sockaddr_in server_address; // internet address object, default IPv4 0.0.0.0:8080
    std::unordered_map<int, Client*> clients; // client file descriptors
    std::list<Request*> requests;
    pollfd polls[10];
    int poll_cnt;
    bool running = false;
    std::mutex stopped;
    pollfd* preparePolls()
    {
        polls[0].fd = server_socket;
        polls[0].events = poll_events;
        polls[0].revents = 0;
        int i = 1;
        for(auto iter = clients.begin(); iter != clients.end(); iter++)
        {
            if(i >= 10)
                break;
            polls[i++] = {iter->second->socket_fd, poll_events, 0};
        }
        poll_cnt = i;
        return polls;
    }
    /**
     * @brief Construct a new HTTPServer object.
     * 
     * @param port The port to listen to. If none is provided, assume 8080.
     */
    HTTPServer(int port = 8080)
    {
        server_socket = socket(AF_INET, SOCK_STREAM, 0); // TCP IP with "custom" protocol
    
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(port);
        server_address.sin_addr.s_addr = INADDR_ANY;
    }

    /**
     * @brief Start handling of clients
     * 
     */
    void begin()
    {
        stopped.lock();
        if(running)
        {
            stopped.unlock();
            return;
        }
        running = true;
        if(bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) != 0)
        {
            std::cout<<"Bind failed"<<"\n";
            return;
        }
        if(listen(server_socket, 5) != 0)
        {
            std::cout<<"Listen failed"<<"\n";
            return;
        }
        std::thread thread(&HTTPServer::loop_wrap, this);
        std::cout<<"Server initialized successfuly"<<"\n";
        thread.detach();
    }

    void end()
    {
        if(!running)
            return;
        running = false;
        stopped.lock(); // this is a most unholy creation
        stopped.unlock();
    }

    void loop()
    {
        // poll(the listening socket + all the clients and parse the requests)
        this->preparePolls();
        int ready = poll(polls, poll_cnt, 100);
        if(ready == -1)
            throw std::exception();
        if(polls[0].revents != 0)
        {
            ready--;
            if(polls[0].revents == POLLIN)
                addNextClient();
            else
                throw std::exception(); // shit has hit the fan
        }
        for(int i = 1; i < poll_cnt; i++)
        {
            if(ready == 0)
                break;
            if(polls[i].revents == 0)
                continue;
            if(polls[i].revents == POLLIN)
                processClient(polls[i].fd);
            else
                removeClient(polls[i].fd);
        }
        for(auto iter = requests.begin(); iter != requests.end(); ++iter)
        {
            Request& request = (**iter);
            std::cout << "Responce to " << request.response_fd << ": " << request.text;
            send(request.response_fd, request.text.c_str(), request.text.length(), 0); //delete request
            if(request.headers["Connection"] == "close")
                removeClient(request.response_fd);
        }
        requests.clear();
    }
    
    /**
     * @brief Process TCP client data, turning it into HTTP request.
     * 
     */
    void processClient(int client_fd)
    {
        Client& client = *clients[client_fd]; 
        Request::Status status = client.update();
        if(status == Request::Status::TOO_LARGE)
        {
            Response::tooLarge(client_fd).respond();
            removeClient(client_fd);
        }
        else if(status == Request::Status::MALFORMED)
        {
            Response::badRequest(client_fd).respond();
            removeClient(client_fd);
        }
        else if(status == Request::Status::VALID)
        {
            requests.push_back(client.extractRequest());
        }
    }

    /**
     * @brief Accepts the next TCP connection in line. Call only when 'server_socket' is ready.
     * 
     */
    void addNextClient()
    {
        int client_fd = accept(server_socket, nullptr, nullptr);
        clients[client_fd] = new Client(client_fd);
    }

    /**
     * @brief s.e.
     * 
     * @param client_fd 
     */
    void removeClient(int client_fd)
    {
        close(client_fd);
        delete clients.at(client_fd);
        clients.erase(client_fd);
    }

    ~HTTPServer()
    {
        close(server_socket);
    }
};

int main(void)
{
    HTTPServer myServer;
    myServer.begin();
    string a;
    std::cin>>a;
    myServer.end();
}
