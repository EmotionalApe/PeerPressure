#ifndef TRACKER_H
#define TRACKER_H

#include <string>
#include <vector>
#include <cstdint>

struct Peer {
    std::string ip;
    uint16_t port;
};

class Tracker {
public:
    Tracker(const std::string& announce_url, const std::string& info_hash, int64_t total_length);
    std::vector<Peer> get_peers();

private:
    std::string announce_url;
    std::string info_hash;
    int64_t total_length;
    std::string peer_id;

    std::string build_url();
};

#endif // TRACKER_H
