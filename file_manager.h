#pragma once

#include "bencode.h"

#include <vector>
#include <string>

class FileManager {
public:

    static bool reconstruct_files(
        const BencodeValue& info_dict,
        const std::vector<unsigned char>& torrent_data
    );
};