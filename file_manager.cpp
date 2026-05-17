#include "file_manager.h"

#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

bool FileManager::reconstruct_files(
    const BencodeValue& info_dict,
    const std::vector<unsigned char>& torrent_data
) {

    uint64_t current_offset = 0;

    std::string root_name =
        info_dict._dict_val.at("name")._str_val;

    // multi-file torrent
    if (info_dict._dict_val.count("files")) {

        auto& files =
            info_dict._dict_val.at("files")._list_val;

        for (const auto& file_entry : files) {

            auto& file_dict =
                file_entry._dict_val;

            uint64_t file_length =
                file_dict.at("length")._int_val;

            // build path
            fs::path output_path = root_name;

            auto& path_list =
                file_dict.at("path")._list_val;

            for (const auto& part : path_list) {
                output_path /=
                    part._str_val;
            }

            // create directories
            fs::create_directories(
                output_path.parent_path()
            );

            // bounds check
            if (current_offset + file_length >
                torrent_data.size()) {

                std::cerr << "Invalid file bounds\n";
                return false;
            }

            // write file
            std::ofstream out(
                output_path,
                std::ios::binary
            );

            out.write(
                reinterpret_cast<const char*>(
                    torrent_data.data()
                    + current_offset
                ),
                file_length
            );

            out.close();

            std::cout << "Created: "
                      << output_path
                      << " ("
                      << file_length
                      << " bytes)\n";

            current_offset += file_length;
        }
    }

    // single-file torrent
    else {

        uint64_t file_length =
            info_dict._dict_val.at("length")._int_val;

        std::string file_name =
            info_dict._dict_val.at("name")._str_val;

        std::ofstream out(
            file_name,
            std::ios::binary
        );

        out.write(
            reinterpret_cast<const char*>(
                torrent_data.data()
            ),
            file_length
        );

        out.close();

        std::cout << "Created: "
                  << file_name
                  << "\n";
    }

    return true;
}