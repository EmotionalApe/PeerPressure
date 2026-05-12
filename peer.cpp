#include "peer.h"

#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/socket.h>
#endif

PeerConnection::PeerConnection(const std::string& ip, uint16_t port)
    : ip(ip), port(port) {}

bool PeerConnection::connect_to_peer() {

#ifdef _WIN32
    static bool wsa_initialized = false;

    if (!wsa_initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2), &wsa);
        wsa_initialized = true;
    }
#endif

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (sockfd == INVALID_SOCKET) {
#else
    if (sockfd < 0) {
#endif
        std::cerr << "Failed to create socket\n";
        return false;
    }

    // Set socket timeouts
#ifdef _WIN32
    DWORD timeout = 5000; // 5 seconds in ms
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address\n";
        return false;
    }

    std::cout << "Connecting to " << ip << ":" << port << "...\n";

#ifdef _WIN32
    if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
#else
    if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
#endif
        std::cerr << "Connection failed\n";
        return false;
    }

    std::cout << "Connected!\n";
    return true;
}

bool PeerConnection::send_handshake(
    const std::vector<unsigned char>& info_hash,
    const std::string& peer_id
) {
    std::vector<unsigned char> handshake;

    // protocol length
    handshake.push_back(19);

    // protocol string
    std::string protocol = "BitTorrent protocol";
    handshake.insert(handshake.end(), protocol.begin(), protocol.end());

    // reserved bytes
    handshake.insert(handshake.end(), 8, 0);

    // info hash
    handshake.insert(handshake.end(), info_hash.begin(), info_hash.end());

    // peer id
    handshake.insert(handshake.end(), peer_id.begin(), peer_id.end());

    int sent = send(sockfd,
        reinterpret_cast<const char*>(handshake.data()),
        handshake.size(),
        0);

    if (sent != handshake.size()) {
        std::cerr << "Failed to send full handshake\n";
        return false;
    }

    std::cout << "Handshake sent!\n";
    return true;
}

bool PeerConnection::receive_handshake(
    const std::vector<unsigned char>& expected_info_hash
) {
    unsigned char response[68];

    int received = recv(sockfd, reinterpret_cast<char*>(response), 68, 0);

    if (received != 68) {
        std::cerr << "Invalid handshake response\n";
        return false;
    }

    // validate protocol length
    if (response[0] != 19) {
        std::cerr << "Invalid protocol length\n";
        return false;
    }

    // validate protocol string
    std::string protocol(
        reinterpret_cast<char*>(response + 1),
        19
    );

    if (protocol != "BitTorrent protocol") {
        std::cerr << "Invalid protocol string\n";
        return false;
    }

    // validate info hash
    for (int i = 0; i < 20; i++) {
        if (response[28 + i] != expected_info_hash[i]) {
            std::cerr << "Info hash mismatch\n";
            return false;
        }
    }

    std::cout << "Handshake successful!\n";
    return true;
}

void PeerConnection::close_connection() {

#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif

    std::cout << "Connection closed\n";
}