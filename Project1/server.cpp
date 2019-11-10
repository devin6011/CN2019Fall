#include "utils.h"
#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

int startServer(unsigned short listenPort) {
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(listenSocket < 0) {
        std::perror("socket creation error");
        return 1;
    }

    sockaddr_in socketAddress;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(listenPort);
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    auto bindStatus = bind(listenSocket,
                           reinterpret_cast<sockaddr*>(&socketAddress),
                           sizeof(socketAddress));
    if(bindStatus < 0) {
        std::perror("bind error");
        return 1;
    }

    if(listen(listenSocket, 128) < 0) {
        std::perror("listen error");
        return 1;
    }

    std::map<int, sockaddr_in> clients;

    while(true) {
        fd_set readFDSet;
        FD_ZERO(&readFDSet);
        FD_SET(listenSocket, &readFDSet);
        for(const auto &client : clients)
            FD_SET(client.first, &readFDSet);

        auto selectStatus = select(FD_SETSIZE, &readFDSet,
                                   nullptr, nullptr, nullptr);
        
        if(selectStatus < 0) {
            perror("select error");
            return 1;
        }
        else if(selectStatus == 0) {
            continue;
        }

        if(FD_ISSET(listenSocket, &readFDSet)) {
            sockaddr_in clientAddress;
            auto clientAddressLen =
                            static_cast<socklen_t>(sizeof(clientAddress));
            int clientFD;
            clientFD = accept(listenSocket,
                              reinterpret_cast<sockaddr*>(&clientAddress),
                              &clientAddressLen);
            if(clientFD < 0) {
                perror("accept error");
                return 1;
            }
            clients.emplace(clientFD, clientAddress);
        }

        std::vector<int> closedSockets;
        for(const auto &client : clients) {
            const auto clientFD = client.first;
            const auto &clientAddress = client.second;
            if(FD_ISSET(clientFD, &readFDSet)) {
                int32_t data;
                auto recvStatus = recv(clientFD, &data, sizeof(data),
                                       MSG_WAITALL);
                if(recvStatus < 0) {
                    if(errno != ECONNRESET) {
                        perror("recv error");
                        return 1;
                    }
                }
                else if(recvStatus == 0) {
                    closedSockets.push_back(clientFD);
                    close(clientFD);
                }
                else {
                    std::cout << "recv from "
                              << inet_ntoa(clientAddress.sin_addr)
                              << ':'
                              << ntohs(clientAddress.sin_port)
                              << std::endl;
                    auto sendStatus = send(clientFD, &data,
                                           sizeof(data), MSG_NOSIGNAL);
                    if(sendStatus < 0) {
                        if(errno != ECONNRESET) {
                            perror("send error");
                            return 1;
                        }
                    }
                }
            }
        }
        for(auto closedFD : closedSockets)
            clients.erase(closedFD);
    }

    close(listenSocket);

    return 0;
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        std::cout << "Usage: ./server listen_port" << std::endl;
        return 1;
    }

    unsigned short listenPort;

    if(!string2ushort(argv[1], listenPort)) {
        std::cout << "Error: invalid listen port" << std::endl;
        return 2;
    }

    return startServer(listenPort);
}
