#pragma once

#include <vector>
#include <string>
#include <cstdint>

class PieceManager {
private:
    std::string pieces_blob;
    uint32_t piece_length;
    std::vector<bool> completed_pieces;

public:
    PieceManager(
        const std::string& pieces_blob,
        uint32_t piece_length
    );

    bool verify_piece(
        uint32_t piece_index,
        const std::vector<unsigned char>& piece_data
    );

    bool is_piece_complete(uint32_t piece_index) const;
    void mark_piece_complete(uint32_t piece_index);
};