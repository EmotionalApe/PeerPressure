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
    struct PeerMessage{
        int id; 
        std::vector<unsigned char> payload;
        bool valid; 
    };

    PeerConnection(const std::string& ip, uint16_t port);
    bool connect_to_peer();
    bool send_handshake(
        const std::vector<unsigned char>& info_hash,
        const std::string& peer_id
    );

    bool receive_handshake(
        const std::vector<unsigned char>& expected_info_hash
    );

    PeerMessage receive_message(); 

    bool send_interested();

    bool send_request(
        uint32_t index,
        uint32_t begin,
        uint32_t length
    );

    void close_connection();
};