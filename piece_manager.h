#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <mutex>

class PieceManager {
private:
    std::string pieces_blob;
    uint32_t piece_length;
    std::vector<bool> completed_pieces;
    std::vector<unsigned char> torrent_data_;
    mutable std::mutex mutex_;

public:
    PieceManager(
        const std::string& pieces_blob,
        uint32_t piece_length
    );

    void initialize_buffer(int64_t total_length);

    bool verify_piece(
        uint32_t piece_index,
        const std::vector<unsigned char>& piece_data
    );

    bool write_piece(
        uint32_t piece_index,
        const std::vector<unsigned char>& piece_data
    );

    const std::vector<unsigned char>& get_torrent_data() const;

    bool is_piece_complete(uint32_t piece_index) const;
    void mark_piece_complete(uint32_t piece_index);
};