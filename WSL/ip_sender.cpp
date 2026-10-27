// ip_sender.cpp
// WSL(우분투 22.04)에서 실행.
// 터미널에 "호출"을 입력하면 내 IP를 Jetson으로 UDP 전송한다.
// ROS2와 무관한 일반 소켓 프로그램.
//
// 빌드: g++ -std=c++17 -o ip_sender ip_sender.cpp
// 실행: ./ip_sender <Jetson IP> <포트>
// 예:  ./ip_sender 192.168.0.50 5000

#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netinet/in.h>

// loopback을 제외한 첫 번째 IPv4 주소를 찾는다.
std::string getLocalIP()
{
    struct ifaddrs *ifaddr, *ifa;
    std::string result;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        std::string name(ifa->ifa_name);
        if (name == "lo") continue;

        void *addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, addr, buf, sizeof(buf));
        result = buf;
        break; // 첫 번째로 찾은 non-loopback IPv4 사용
    }

    freeifaddrs(ifaddr);
    return result;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "사용법: " << argv[0] << " <Jetson IP> <포트>\n";
        return 1;
    }

    std::string jetsonIP = argv[1];
    int port = std::stoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, jetsonIP.c_str(), &servaddr.sin_addr) <= 0) {
        std::cerr << "잘못된 IP 주소입니다: " << jetsonIP << "\n";
        return 1;
    }

    std::cout << "\"호출\" 입력 시 내 IP를 Jetson(" << jetsonIP << ":" << port
              << ")으로 전송합니다. 종료: Ctrl+C\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "호출") {
            std::string myIP = getLocalIP();
            if (myIP.empty()) {
                std::cerr << "IP 확인 실패\n";
                continue;
            }
            ssize_t sent = sendto(sockfd, myIP.c_str(), myIP.size(), 0,
                                   (struct sockaddr*)&servaddr, sizeof(servaddr));
            if (sent < 0) {
                perror("sendto");
            } else {
                std::cout << "[전송됨] " << myIP << " -> " << jetsonIP << ":" << port << "\n";
            }
        }
    }

    close(sockfd);
    return 0;
}