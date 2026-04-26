#pragma once
#include <vector>
#include <map>
#include <string>
#include <cstdint>

struct BencodeValue;  

enum class BencodeType {
    Integer,
    String,
    List,
    Dictionary
};

class BencodeValue {
private:
    BencodeType _type;
    int64_t _int_val;
    std::string _str_val;
    std::vector<BencodeValue> _list_val;
    std::map<std::string, BencodeValue> _dict_val;

public:
    // Constructors (Simplified Style)
    BencodeValue() {
        _type = BencodeType::Integer;
        _int_val = 0;
    }

    BencodeValue(int64_t v) {
        _type = BencodeType::Integer;
        _int_val = v;
    }

    BencodeValue(std::string v) {
        _type = BencodeType::String;
        _str_val = v;
    }

    BencodeValue(const char* v) {
        _type = BencodeType::String;
        _str_val = v;
    }

    // Type checker
    BencodeType type() const { return _type; }

    // Friends allow these functions to access private data
    friend BencodeValue parse_any(const std::string &data, size_t &i);
    friend BencodeValue parse_int(const std::string &data, size_t &i);
    friend BencodeValue parse_string(const std::string &data, size_t &i);
    friend BencodeValue parse_list(const std::string &data, size_t &i);
    friend BencodeValue parse_dict(const std::string &data, size_t &i);
    friend void print_bencode(const BencodeValue &val, int indent);
    friend std::string bencode(const BencodeValue &val);
};


BencodeValue parse_any (const std::string &data, size_t &i);
BencodeValue parse_int (const std::string &data, size_t &i);
BencodeValue parse_string (const std::string &data, size_t &i);
BencodeValue parse_list (const std::string &data, size_t &i);
BencodeValue parse_dict (const std::string &data, size_t &i);

void print_bencode(const BencodeValue &val, int indent=0);
std::string bencode(const BencodeValue &val);
