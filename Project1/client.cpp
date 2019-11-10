#include "utils.h"
#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

bool parseCommandLineArgs(const int argc,
                          char *argv[],
                          int &number,
                          int &timeout,
                          std::vector<sockaddr_in> &serverList) {
    // Default values
    number = 0;
    timeout = 1000;

    // Clear old values
    std::vector<sockaddr_in>().swap(serverList);

    for(int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if(arg == "-n") {
            ++i;
            if(!(i < argc))
                return false;
            std::string arg2(argv[i]);

            if(!string2int(arg2, number))
                return false;
        }
        else if(arg == "-t") {
            ++i;
            if(!(i < argc))
                return false;
            std::string arg2(argv[i]);

            if(!string2int(arg2, timeout))
                return false;
        }
        else {
            auto sepPos = arg.find(':');
            if(sepPos == std::string::npos)
                return false;

            auto ipString = arg.substr(0, sepPos);
            auto portString = arg.substr(sepPos+1);

            auto host = gethostbyname(ipString.data());
            if(!host)
                return false;

            auto ip = reinterpret_cast<in_addr*>(host->h_addr)->s_addr;

            unsigned short port;
            if(!string2ushort(portString, port))
                return false;

            sockaddr_in socketAddress;
            socketAddress.sin_family = AF_INET;
            socketAddress.sin_port = htons(port);
            socketAddress.sin_addr.s_addr = ip;

            serverList.push_back(socketAddress);
        }
    }

    return true;
}

void printTimeoutMessage(const sockaddr_in &server) {
    std::cout << std::string("timeout when connect to ") +
                 std::string(inet_ntoa(server.sin_addr)) +
                 std::string(":") +
                 std::to_string(ntohs(server.sin_port)) +
                 std::string("\n");
    std::cout.flush();
}

void pingServer(const sockaddr_in &server,
                const int number,
                const int timeout) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    timeval socketTimeout;
    socketTimeout.tv_sec = timeout / 1000;
    socketTimeout.tv_usec = timeout * 1000 % 1000000;
    if(setsockopt(serverSocket, SOL_SOCKET, SO_SNDTIMEO,
                  &socketTimeout, sizeof(socketTimeout)) < 0) {
        perror("setsockopt error");
        close(serverSocket);
        return;
    }
    if(setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO,
                  &socketTimeout, sizeof(socketTimeout)) < 0) {
        perror("setsockopt error");
        close(serverSocket);
        return;
    }

    auto connectStatus = connect(serverSocket,
                                 reinterpret_cast<const sockaddr*>(&server),
                                 sizeof(server));
    if(connectStatus < 0) {
        if(errno == EWOULDBLOCK or
           errno == ECONNREFUSED or
           errno == EINPROGRESS)
            printTimeoutMessage(server);
        else
            perror("connect error");
        close(serverSocket);
        return;
    }

    for(int packetCnt = 0; packetCnt < number or number == 0; ++packetCnt) {
        if(packetCnt != 0)
            std::this_thread::sleep_for(std::chrono::seconds(1));
        int32_t data = packetCnt;
        auto startTime = std::chrono::steady_clock::now();
        if(send(serverSocket, &data, sizeof(data), MSG_NOSIGNAL) < 0) {
            if(errno == EPIPE)
                printTimeoutMessage(server);
            else
                perror("send error");
            close(serverSocket);
            return;
        }
        int32_t recvData = ~data;
        do {
            auto recvStatus = recv(serverSocket, &recvData, sizeof(recvData),
                                   MSG_WAITALL);
            if(recvStatus < 0) {
                if(errno == EWOULDBLOCK) {
                    break;
                }
                else if(errno == ECONNRESET) {
                    printTimeoutMessage(server);
                    close(serverSocket);
                    return;
                }
                else {
                    perror("recv error");
                    close(serverSocket);
                    return;
                }
            }
            else if(recvStatus == 0) {
                printTimeoutMessage(server);
                close(serverSocket);
                return;
            }
        } while(recvData != data);

        if(recvData != data) {
            printTimeoutMessage(server);
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto diff = now - startTime;
        using ms = std::chrono::milliseconds;
        auto timeSpan = std::chrono::duration_cast<ms>(diff).count();

        if(timeSpan > timeout) {
            printTimeoutMessage(server);
        }
        else {
            std::cout << std::string("recv from ") +
                         std::string(inet_ntoa(server.sin_addr)) +
                         std::string(":") +
                         std::to_string(ntohs(server.sin_port)) +
                         std::string(", RTT = ") +
                         std::to_string(timeSpan) +
                         std::string(" msec\n");
            std::cout.flush();
        }
    }

    close(serverSocket);
}

void ping(const std::vector<sockaddr_in> &serverList,
          const int number,
          const int timeout) {
    std::vector<std::thread> threads;
    for(const auto &server : serverList)
        threads.push_back(std::thread(pingServer, server, number, timeout));
    for(auto &thread : threads)
        thread.join();
}

int main(int argc, char *argv[]) {
    int number, timeout;
    std::vector<sockaddr_in> serverList;

    if(!parseCommandLineArgs(argc, argv, number, timeout, serverList)) {
        std::cout << "Usage: ./client [-n number] [-t timeout] "
                     "host_1:port_1 host_2:port_2 ..." << std::endl;
        return 1;
    }

    ping(serverList, number, timeout);

    return 0;
}
