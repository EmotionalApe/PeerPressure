#pragma once

#include <string>
#include <vector> 
#include <cstdint>

#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
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
    std::vector<bool> available_pieces;
    bool peer_choking = true;
    bool peer_interested = false;

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

    void parse_bitfield (const std::vector<unsigned char> &payload); 
    bool has_piece(uint32_t piece_index) const; 
    void handle_have(const std::vector<unsigned char> &payload);
    void process_message(const PeerMessage& msg);
    bool is_choking() const;
    bool is_readable(int timeout_ms);

    void close_connection();
};