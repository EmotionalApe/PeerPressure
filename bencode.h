#pragma once

#include <variant>
#include <vector>
#include <map>
#include <string>
#include <cstdint>

struct BencodeValue;  

using BencodeInt = int64_t;
using BencodeString = std::string;
using BencodeList = std::vector<BencodeValue>;
using BencodeDict = std::map<std::string, BencodeValue>;

struct BencodeValue : std::variant<BencodeInt, BencodeString, BencodeList, BencodeDict> {
    using variant::variant;  //inherit all constructors from variant
};


BencodeValue parse_any (const std::string &data, size_t &i);
BencodeValue parse_int (const std::string &data, size_t &i);
BencodeValue parse_string (const std::string &data, size_t &i);
BencodeValue parse_list (const std::string &data, size_t &i);
BencodeValue parse_dict (const std::string &data, size_t &i);
