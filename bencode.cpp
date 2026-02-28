#include "bencode.h"
#include <stdexcept>
#include <cctype>


BencodeValue parse_any(const std::string &data, size_t &i) {
    if (i>=data.size()) {
        throw std::runtime_error("Invalid bencode: unexpected end of data");
    }
    char c = data[i];

    if (isdigit(c)) {
        return parse_string(data, i);
    } else if (c == 'i') {
        return parse_int(data, i);
    } else if (c == 'l') {
        return parse_list(data, i);
    } else if (c == 'd') {
        return parse_dict(data, i);
    } else {
        throw std::runtime_error("Invalid bencode: unexpected character");
    }   
}

BencodeValue parse_string(const std::string &data, size_t &i) {
    size_t colon = data.find(':', i);
    if (colon == std::string::npos) {
        throw std::runtime_error("Invalid bencode string: missing colon");
    }
    
    size_t length = std::stoull(data.substr(i, colon - i));
    i = colon + 1;
    
    std::string str=data.substr(i, length);
    i += length;
    return str;
}

BencodeValue parse_int(const std::string &data, size_t &i) {
    if (data[i]!='i') {
        throw std::runtime_error("Invalid bencode int: missing 'i'");
    }
    i++;

    size_t end = data.find('e', i);
    if (end == std::string::npos) {
        throw std::runtime_error("Invalid bencode int: missing 'e'");
    }
    int64_t value = std::stoll(data.substr(i, end - i));
    i = end + 1;
    return value;
}

BencodeValue parse_list(const std::string& data, size_t& i) {
    if (data[i]!='l') {
        throw std::runtime_error("Invalid bencode list: missing 'l'");
    }
    i++;

    BencodeList list;
    while (i<data.size() && data[i]!='e'){
        list.push_back(parse_any(data,i));
    }

    if (i>=data.size()) {
        throw std::runtime_error("Invalid bencode list: missing 'e'");
    }
    i++;
    return list;
}

BencodeValue parse_dict(const std::string& data, size_t& i) {
    if (data[i]!='d') {
        throw std::runtime_error("Invalid bencode dict: missing 'd'");
    }
    i++;

    BencodeDict dict;
    while (i<data.size() && data[i]!='e'){
        BencodeValue key_val = parse_string(data,i);
        std::string key = std::get<std::string>(key_val);
        
        BencodeValue value = parse_any(data,i);
        dict[key] = value;
    }

    if (i>=data.size()) {
        throw std::runtime_error("Invalid bencode dict: missing 'e'");
    }
    i++;
    return dict;
}


