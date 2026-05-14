#include "peer.h"
#include "utils.h"

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
    DWORD timeout = 30000; // 30 seconds in ms
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
    int total_received = 0;

    while (total_received < 68) {
        int r = recv(sockfd, 
                     reinterpret_cast<char*>(response) + total_received, 
                     68 - total_received, 
                     0);
        if (r <= 0) {
            if (r == 0) {
                std::cerr << "Peer closed connection during handshake\n";
            } else {
                #ifdef _WIN32
                std::cerr << "Socket error during handshake. Error Code: " << WSAGetLastError() << "\n";
                #else
                std::cerr << "Socket error during handshake. errno: " << errno << "\n";
                #endif
            }
            return false;
        }
        total_received += r;
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

PeerConnection::PeerMessage PeerConnection::receive_message() {

    PeerMessage msg;
    msg.valid = false; 
    unsigned char length_buf[4];
    int total_received = 0;

    while (total_received < 4) {
        int r = recv(sockfd,
                    reinterpret_cast<char*>(length_buf) + total_received,
                    4 - total_received,
                    0);
        if (r <= 0) {
            if (r == 0) {
                std::cerr << "Peer closed connection while reading length\n";
            } else {
                #ifdef _WIN32
                std::cerr << "Socket error while reading length. Error Code: " << WSAGetLastError() << "\n";
                #else
                std::cerr << "Socket error while reading length. errno: " << errno << "\n";
                #endif
            }
            return msg;
        }
        total_received += r;
    }

    uint32_t length = utils::read_uint32_be(length_buf);

    // keepalive
    if (length == 0) {
        msg.id = -2;
        msg.valid = true; 
        return msg; 
    }

    if (length > 1024 * 1024) { // Safety check: 1MB max message for now
        std::cerr << "Message too large: " << length << "\n";
        return msg;
    }

    // 2. Read exactly 1 byte for the message ID
    unsigned char message_id;
    int r = recv(sockfd, reinterpret_cast<char*>(&message_id), 1, 0);
    if (r != 1) {
        std::cerr << "Failed to read message id\n";
        return msg;
    }
    msg.id = static_cast<int>(message_id);

    // 3. Read the entire payload
    int payload_length = length - 1;
    if (payload_length > 0) {
        msg.payload.resize(payload_length); 
        int payload_received = 0;
        
        while (payload_received < payload_length) {
            int r = recv(sockfd,
                        reinterpret_cast<char*>(msg.payload.data()) + payload_received,
                        payload_length - payload_received,
                        0);
            if (r <= 0) {
                std::cerr << "Failed to read full payload\n";
                return msg;
            }
            payload_received += r;
        }
    }
    msg.valid = true; 
    return msg;
}

bool PeerConnection::send_interested() {

    unsigned char msg[5] = {
        0, 0, 0, 1, // length prefix
        2            // interested
    };

    int sent = send(sockfd,
        reinterpret_cast<char*>(msg),
        5,
        0);

    if (sent != 5) {
        std::cerr << "Failed to send interested message\n";
        return false;
    }

    std::cout << "Interested message sent\n";

    return true;
}

bool PeerConnection::send_request(
    uint32_t piece_index,
    uint32_t begin,
    uint32_t length
) {

    std::vector<unsigned char> msg;

    // total payload length = 13
    uint32_t msg_length = htonl(13);

    // append length prefix
    msg.insert(msg.end(),
        reinterpret_cast<unsigned char*>(&msg_length),
        reinterpret_cast<unsigned char*>(&msg_length) + 4);

    // message id = 6 (request)
    msg.push_back(6);

    // piece index
    uint32_t piece_be = htonl(piece_index);

    msg.insert(msg.end(),
        reinterpret_cast<unsigned char*>(&piece_be),
        reinterpret_cast<unsigned char*>(&piece_be) + 4);

    // begin offset
    uint32_t begin_be = htonl(begin);

    msg.insert(msg.end(),
        reinterpret_cast<unsigned char*>(&begin_be),
        reinterpret_cast<unsigned char*>(&begin_be) + 4);

    // block length
    uint32_t length_be = htonl(length);

    msg.insert(msg.end(),
        reinterpret_cast<unsigned char*>(&length_be),
        reinterpret_cast<unsigned char*>(&length_be) + 4);

    int sent = send(sockfd,
        reinterpret_cast<const char*>(msg.data()),
        msg.size(),
        0);

    if (sent != msg.size()) {
        std::cerr << "Failed to send request message\n";
        return false;
    }

    std::cout << "Requested piece "
              << piece_index
              << ", offset "
              << begin
              << ", length "
              << length
              << "\n";

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