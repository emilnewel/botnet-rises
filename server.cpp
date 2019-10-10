//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000
//
// Author: Jacky Mallett (jacky@ru.is)
//
// GROUP : 2
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
    std::string groupName; // Limit length of name of client's user
    std::string ip;
    std::string port;

    Client(int socket, std::string ip, std::string port, std::string groupName)
    {
        this->sock = socket;
        this->port = port;
        this->groupName = groupName;
        this->ip = ip;
        
    }

    ~Client() {} // Virtual destructor defined for base class
};

// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table,
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Client *> clients; // Lookup table for per Client information

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

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

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SO_REUSEADDR:");
    }
    set = 1;
#ifdef __APPLE__
    if (setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SOCK_NOBBLOCK");
    }
#endif
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
    else
    {
        return (sock);
    }
}

// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.

void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{
    // Remove client from the clients list
    // If this client's socket is maxfds then the next lowest
    // one has to be determined. Socket fd's can be reused by the Kernel,
    // so there aren't any nice ways to do this.
    if (*maxfds == clientSocket)
    {
        for (auto const &p : clients)
        {
            *maxfds = std::max(*maxfds, p.second->sock);   
        }
    }
    // And remove from the list of open sockets.

    FD_CLR(clientSocket, openSockets);
}

void CONNECT(sockaddr_in server_addr, std::string address, int port)
{
    int outSock = socket(AF_INET, SOCK_STREAM, 0);
    hostent *server = gethostbyname(address.c_str());
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
    if (connect(outSock, (struct sockaddr *)&server_addr, sizeof(server_addr)) >= 0)
    {
        std::cout << "Connected" << std::endl;
        Client* c = new Client(outSock,address,std::to_string(port), "V_GROUP_2");
        clients[outSock] = c;
        
    }
    else
    {
        std::cout << "Connect failed" << std::endl;
    }
}
std::string LISTSERVERS()
{
    std::string str = "SERVERS,";
    for(auto const& c : clients)
    {
        Client* cl = c.second;
        str += cl->groupName + "," + cl->ip + "," + cl->port + ";";
    }
    std::cout << str << std::endl;
    return str;
}
void SERVERS()
{
}
void KEEPALIVE()
{
}
void GET_MSG()
{
}
void SEND_MSG(std::string groupName, std::string msg)
{
    for(auto const& c : clients)
    {
        if(c.second->groupName == groupName){
            send(c.second->sock, msg, strlen(msg), 0);
        }
    }
}

void LEAVE(std::string port, fd_set *openSockets, int *maxfds)
{
    for(const auto &c : clients)
    {
        std::cout << "PORT: " << port << " CLIENT SOCK: " << c.second->port << std::endl;
        std::cout << *maxfds;
        if(c.second->port == port)
        {
            Client *cl = c.second;
            //clients.erase(cl->sock);   
            if (*maxfds == cl->sock)
            {
                std::cout << "hello from if" << std::endl; 
                for (auto const &p : clients)
                {
                    std::cout << "hello from for" << std::endl;
                    *maxfds = std::max(*maxfds, p.second->sock);   
                }   
            }
             
        }
    }

}
void STATUSREQ()
{
}
void STATUSRESP()
{
}

void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds, char *buffer, Client *client)
{
    std::vector<std::string> tokens;
    std::string token;
    char msg[1000];

    std::stringstream stream(buffer);
    {
        tokens.push_back(token);
    }

    std::cout << tokens[0] << std::endl;
    if (tokens[0].compare("CONNECT") == 0 && tokens.size() == 3)
    {
        struct sockaddr_in sk_addr;
        CONNECT(sk_addr, tokens[1], stoi(tokens[2]));
    }
    else if (tokens[0].compare("LISTSERVERS") == 0)
    {
        strcpy(msg, LISTSERVERS().c_str());
        send(clientSocket, msg, strlen(msg), 0);
    }
    else if (tokens[0].compare("KEEPALIVE") == 0)
    {
        //TODO: IMPLEMENT
        KEEPALIVE();
    }
    else if (tokens[0].compare("GET_MSG") == 0)
    {
        std::cout << "Who is logged on" << std::endl;
        std::string msg;

        for (auto const &names : clients)
        {
            msg += names.second->groupName + ",";
        }
        // Reducing the msg length by 1 loses the excess "," - which
        // granted is totally cheating.
        send(clientSocket, msg.c_str(), msg.length() - 1, 0);
    }
    else if (tokens[0].compare("SEND_MSG") == 0)
    {
        //TODO: IMPLEMENT
        SEND_MSG(tokens[2],tokens[3]);
    }
    else if (tokens[0].compare("LEAVE") == 0)
    {
        //TODO: IMPLEMENT
        LEAVE(tokens[2], openSockets, maxfds);
    }
    else if (tokens[0].compare("STATUSREQ") == 0)
    {
        //TODO: IMPLEMENT
        STATUSREQ();
    }
    else if (tokens[0].compare("STATUSRESP") == 0)
    {
        //TODO: IMPLEMENT
        STATUSRESP();
    }
    else
    {
        //std::string msg = "Command " + buffer + "has not yet been implemented, check back later :) ";
        //send(clientSocket, msg.c_str(), msg.length() - 1, 0);
        std::cout << "Unknown command from connection:" << buffer << std::endl;
    }
}

int main(int argc, char *argv[])
{
    bool finished;
    int listenSock;       // Socket for connections to server
    int clientSock;       // Socket of connecting client
    fd_set openSockets;   // Current open sockets
    fd_set readSockets;   // Socket list for select()
    fd_set exceptSockets; // Exception socket list
    int maxfds;           // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[1025]; // buffer for reading from clients

    if (argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to

    listenSock = open_socket(atoi(argv[1]));
    printf("Listening on port: %d\n", atoi(argv[1]));

    if (listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }
    else
    // Add listen socket to socket set we are monitoring
    {
        FD_ZERO(&openSockets);
        FD_SET(listenSock, &openSockets);
        maxfds = listenSock;
    }

    finished = false;

    while (!finished)
    {
        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

        if (n < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            // First, accept  any new connections to the server on the listening socket
            if (FD_ISSET(listenSock, &readSockets))
            {
                clientSock = accept(listenSock, (struct sockaddr *)&client,
                                    &clientLen);
                printf("accept***\n");
                // Add new client to the list of open sockets
                FD_SET(clientSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = std::max(maxfds, clientSock);

                // create a new client to store information.
                clients[clientSock] = new Client(clientSock, "127.0.0.1", "4001", "Client");

                // Decrement the number of sockets waiting to be dealt with
                n--;

                printf("Client connected on server: %d\n", clientSock);
            }
            // Now check for commands from clients
            while (n-- > 0)
            {
                for (auto const &pair : clients)
                {
                    Client *client = pair.second;

                    if (FD_ISSET(client->sock, &readSockets))
                    {
                        // recv() == 0 means client has closed connection
                        if (recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                        {
                            printf("Client closed connection: %d", client->sock);
                            close(client->sock);

                            closeClient(client->sock, &openSockets, &maxfds);
                        }
                        // We don't check for -1 (nothing received) because select()
                        // only triggers if there is something on the socket for us.
                        else
                        {
                            std::cout << buffer << std::endl;
                            clientCommand(client->sock, &openSockets, &maxfds, buffer, client);
                        }
                    }
                }
            }
        }
    }
}
