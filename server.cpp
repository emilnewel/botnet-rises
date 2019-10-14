//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000
//
// Author: Jacky Mallett (jacky@ru.is)
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG 5 // Allowed length of queue of waiting connections

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
public:
    int sock;         // socket of client connection
    std::string name;


    Client(int socket) : sock(socket) 
    {
        this->sock = socket;
        this->name = "test";
    }

    ~Client() {} // Virtual destructor defined for base class
};

class Server{
    public:
        int sock;
        std::string groupName;
        std::string ip;
        std::string port;
        Server(int socket, std::string ip, std::string port)
        {
            this->sock = socket;
            this->ip = ip;
            this->port = port;
            this->groupName = "";
        }

        ~Server(){}
};
// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table,
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Client* > clients; // Lookup table for connected clients.
std::map<int, Server* > servers; // Lookup table for connected servers;

int open_socket(int portno)
{
    struct sockaddr_in sk_addr; // address settings for bind()
    int sock;                   // socket opened for this port
    int set = 1;                // for setsockopt

    // Create socket for connection. Set to be non-blocking, so recv will
    // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to open socket");
        return (-1);
    }
#else
    if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
    {
        perror("Failed to open socket");
        return (-1);
    }
#endif

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after
    // program exit.
#ifdef __APPLE__
    if (setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SOCK_NOBBLOCK");
    }
#else
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SO_REUSEADDR:");
    }
#endif
    set = 1;

    memset(&sk_addr, 0, sizeof(sk_addr));

    sk_addr.sin_family = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port = htons(portno);

    // Bind to socket to listen for connections from clients
    
    if (bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
    {
        perror("Failed to bind to socket:");
        return (-1);
    }
    return (sock);
}

struct sockaddr_in getSockaddr_in(const char *host, int port) {
    struct hostent *server;
        struct sockaddr_in serv_addr;

    server = gethostbyname(host);
    if (server == NULL) {
        std::cerr << "Error resolving host" << std::endl;
        return serv_addr;
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    bcopy((char *)server->h_addr,
            (char *)&serv_addr.sin_addr.s_addr,
           server->h_length);
    return serv_addr;
}

void closeClient(int clientSocket, fd_set *openSockets)
{
    int clientFd;
    // Remove client from the clients list
    for(const auto& pair: clients)
    {
        if(pair.second->sock == clientSocket)
        {
            clientFd = pair.first;
        }
    }
    // And remove from the list of open sockets.
    FD_CLR(clientSocket, openSockets);
    close(clientSocket);   
    clients.erase(clientFd);
    std::cout << "Client disconnected: " << clientSocket << std::endl;
}

void closeServer(int serverSocket, fd_set *openSockets)
{
    // Remove client from the clients list
    servers.erase(serverSocket);
    // And remove from the list of open sockets.
    FD_CLR(serverSocket, openSockets);
    close(serverSocket);    
}
//Adds SoH and EoT to string before sending
std::string addToString(std::string msg)
{
    return '\1' + msg + '\4';
}

//Splits buffer on commas and returns vector of tokens
std::vector<std::string> splitBuffer(std::string buf)
{
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream stream(buf);

    while (std::getline(stream, token, ','))
    {
        tokens.push_back(token);
    }
    return tokens;
}

//Removes SoH and EoT from string
std::string sanitizeMsg(std::string msg)
{
    std::string cleanMsg;
    if(msg[0] == '\1')
    {
        cleanMsg = msg.substr(1, msg.length() - 2);
        return cleanMsg;    
    }
    else {
        return msg;
    }
}

void CONNECT(std::string ip, std::string port, fd_set &open)
{
    int servSocket;
    struct sockaddr_in sk_addr;

    sk_addr = getSockaddr_in(ip.c_str(), stoi(port));

    servSocket = socket(AF_INET, SOCK_STREAM, 0);
    int n = connect(servSocket, (struct sockaddr *)&sk_addr, sizeof(sk_addr));
    if(n >= 0)
    {
        std::cout << "Succesfully connected to server: " << ip << " on port " << port << std::endl;
        FD_SET(servSocket, &open);
        servers[servSocket] = new Server(servSocket, ip, port);
    }
    else {
        perror("Connection failed");
    }
}

void newClientConnection(int socket, fd_set &open, fd_set &read)
{
    int fd;
    struct sockaddr_in clientAddress;
    socklen_t clientAddr_size;
    
    if(FD_ISSET(socket, &read)) {
        fd = accept(socket, (struct sockaddr *)&clientAddress, &clientAddr_size);
        clients[fd] = new Client(fd);
        FD_SET(fd, &open);
        printf("Client connected on socket: %d\n", socket);
    }
}

void newServerConnection(int sock, fd_set &open, fd_set &read)
{
    int servSocket;
    struct sockaddr_in serverAddress;
    socklen_t serverAddress_size;
    char address[INET_ADDRSTRLEN];
    std::string ip, port, listServers = "LISTSERVERS,P3_GROUP_2",stuffedLS;
    if(FD_ISSET(sock, &read)) 
    {
        if(servers.size() < 5)
        {
            servSocket = accept(sock,(struct sockaddr *)&serverAddress, &serverAddress_size);
            FD_SET(servSocket, &open);
            port = std::to_string(serverAddress.sin_port);
            inet_ntop(AF_INET,&(serverAddress.sin_addr), address, INET_ADDRSTRLEN);
            ip = address;
            servers[servSocket] = new Server(servSocket, ip, port);
            printf("Server connected on socket: %d\n", sock);
            listServers = "LISTSERVERS,P3_GROUP_2";
            stuffedLS = addToString(listServers);
            std::cout << "FROM: " << stuffedLS << std::endl;
            send(servSocket, stuffedLS.c_str(), stuffedLS.length(),0);

        }
    }
}

void handleClientCommand(fd_set &open, fd_set &read)
{
    char buf[1024];
    
    int n;
    for(const auto& pair: clients)
    {
        int sock = pair.second->sock;
        bool isActive = true;
        if(FD_ISSET(sock, &read))
        {
            n = recv(sock, buf, sizeof(buf), MSG_DONTWAIT);
            if (n >= 0)
            {   
                std::string str(buf);
                std::vector<std::string> tokens = splitBuffer(str);

                if(tokens[0].compare("CONNECT") == 0 && tokens.size() == 3)
                {
                    std::string ip = tokens[1];
                    tokens[2].erase(std::remove(tokens[2].begin(), tokens[2].end(), '\n'), tokens[2].end());
                    std::string port = tokens[2];
                    CONNECT(ip, port, open);
                }
            } else {
                isActive = false;
            }
        }
        if(!isActive)
        {
            closeClient(sock, &open);
        }
    }
    
}
std::string LISTSERVERS(int sock)
{
    std::string str = "SERVERS,";
    
    for (auto const &c : servers)
    {
        Server *cl = c.second;
        if(cl->groupName != "P3_GROUP_2")
        {
            str += cl->groupName + "," + cl->ip + "," + cl->port + ";";
        }
        
    }
    std::cout << str << std::endl;
    return addToString(str);
}
void handleServerCommand(fd_set &open_set, fd_set &read_set)
{
    char buf[1024];
    char msg[1024];
    int n;
    for(const auto& pair: servers)
    {
        std::cout << servers.size() << std::endl;
        int sock = pair.second->sock;
        bool isActive = true;
        if(FD_ISSET(sock, &read_set))
        {
            memset(buf, 0, sizeof(buf));
            n = read(sock,buf,sizeof(buf));
            if (n >= 0)
            {
                std::string clean = sanitizeMsg(buf);
                std::vector<std::string> tokens = splitBuffer(clean);
                std::cout << std::endl << "TO: " << clean << std::endl;
                if (tokens[0].compare("LISTSERVERS") == 0)
                {
                    strcpy(msg, LISTSERVERS(sock).c_str());
                    std::cout << "FROM: " << msg << std::endl;
                    send(sock, msg, strlen(msg), 0);
                }
            } else {
                isActive = false;
            }
        }
        if(!isActive)
        {
            closeServer(sock, &open_set);
        }
    }
}

// Process command from client on the server
void clientCommand(int connSocket, fd_set *openSockets, char *buffer)
{
    std::vector<std::string> tokens = splitBuffer(sanitizeMsg(buffer));    
}

int main(int argc, char *argv[])
{
    bool finished;
    int clientSock, serverSock;
    int clientPort, serverPort;
    fd_set openSockets, readSockets;
    char buffer[1025]; // buffer for reading from clients

    if (argc != 3)
    {
        printf("Usage: ./P3_GROUP_2 <serverPort> <clientPort>\n");
        exit(0);
    }
    clientPort = atoi(argv[2]);
    serverPort = atoi(argv[1]);
    // Setup socket for server to listen to
    serverSock = open_socket(serverPort);
    clientSock = open_socket(clientPort);

    if (listen(serverSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %d\n", serverPort);
        return (-1);
    }
    if (listen(clientSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %d\n", clientPort);
        return (-1);
    }

    FD_ZERO(&openSockets);
    FD_SET(clientSock, &openSockets);
    FD_SET(serverSock, &openSockets);
    finished = false;
    while (!finished)
    {
        // Get modifiable copy of readSockets
        readSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        if (select(FD_SETSIZE, &readSockets, NULL, 0, NULL) < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            //Handle new connection from client
            newClientConnection(clientSock, openSockets, readSockets);
            //Handle new connection from server
            newServerConnection(serverSock, openSockets, readSockets);
            //Handle command from connected servers
            handleServerCommand(openSockets, readSockets);  
            //Handle command from connected clients
            handleClientCommand(openSockets, readSockets);
        }
    }
}