#pragma once

#include <string>
#include <vector> 
#include <cstdint>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif

class PeerConnection {
private:
    std::string ip;
    uint16_t port;

#ifdef _WIN32
    SOCKET sockfd;
#else
    int sockfd;
#endif

public:
    PeerConnection(const std::string& ip, uint16_t port);
    bool connect_to_peer();
    bool send_handshake(
        const std::vector<unsigned char>& info_hash,
        const std::string& peer_id
    );

    bool receive_handshake(
        const std::vector<unsigned char>& expected_info_hash
    );

    void close_connection();
};