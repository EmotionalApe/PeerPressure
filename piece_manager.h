#pragma once

#include <vector>
#include <string>
#include <cstdint>

class PieceManager {
private:
    std::string pieces_blob;
    uint32_t piece_length;

public:
    PieceManager(
        const std::string& pieces_blob,
        uint32_t piece_length
    );

    bool verify_piece(
        uint32_t piece_index,
        const std::vector<unsigned char>& piece_data
    );
};