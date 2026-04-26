#include "bencode.h"
#include <stdexcept>
#include <cctype>
#include <iostream>


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
    
    std::string str = data.substr(i, length);
    i += length;
    return BencodeValue(str);
}

BencodeValue parse_int(const std::string &data, size_t &i) {
    if (data[i] != 'i') {
        throw std::runtime_error("Invalid bencode int: missing 'i'");
    }
    i++;

    size_t end = data.find('e', i);
    if (end == std::string::npos) {
        throw std::runtime_error("Invalid bencode int: missing 'e'");
    }
    int64_t value = std::stoll(data.substr(i, end - i));
    i = end + 1;
    return BencodeValue(value);
}

BencodeValue parse_list(const std::string& data, size_t& i) {
    if (data[i] != 'l') {
        throw std::runtime_error("Invalid bencode list: missing 'l'");
    }
    i++;

    BencodeValue val;
    val._type = BencodeType::List;
    while (i < data.size() && data[i] != 'e') {
        val._list_val.push_back(parse_any(data, i));
    }

    if (i >= data.size()) {
        throw std::runtime_error("Invalid bencode list: missing 'e'");
    }
    i++;
    return val;
}

BencodeValue parse_dict(const std::string& data, size_t& i) {
    if (data[i] != 'd') {
        throw std::runtime_error("Invalid bencode dict: missing 'd'");
    }
    i++;

    BencodeValue val;
    val._type = BencodeType::Dictionary;
    while (i < data.size() && data[i] != 'e') {
        BencodeValue key_val = parse_string(data, i);
        std::string key = key_val._str_val;

        val._dict_val[key] = parse_any(data, i);
    }

    if (i >= data.size()) {
        throw std::runtime_error("Invalid bencode dict: missing 'e'");
    }
    i++;
    return val;
}

void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        std::cout << "    ";
    }
}

void print_bencode(const BencodeValue& value, int indent) {
    switch (value._type) {
        case BencodeType::Integer:
            std::cout << value._int_val;
            break;
        case BencodeType::String:
            std::cout << "\"" << value._str_val << "\"";
            break;
        case BencodeType::List:
            std::cout << "[\n";
            for (const auto& item : value._list_val) {
                print_indent(indent + 1);
                print_bencode(item, indent + 1);
                std::cout << ",\n";
            }
            print_indent(indent);
            std::cout << "]";
            break;
        case BencodeType::Dictionary:
            std::cout << "{\n";
            for (const auto& pair : value._dict_val) {
                print_indent(indent + 1);
                std::cout << pair.first << ": ";
                print_bencode(pair.second, indent + 1);
                std::cout << ",\n";
            }
            print_indent(indent);
            std::cout << "}";
            break;
    }
}

std::string bencode(const BencodeValue& value) {
    switch (value._type) {
        case BencodeType::Integer:
            return "i" + std::to_string(value._int_val) + "e";
        case BencodeType::String:
            return std::to_string(value._str_val.size()) + ":" + value._str_val;
        case BencodeType::List: {
            std::string result = "l";
            for (const auto& item : value._list_val) {
                result += bencode(item);
            }
            result += "e";
            return result;
        }
        case BencodeType::Dictionary: {
            std::string result = "d";
            for (const auto& pair : value._dict_val) {
                result += bencode(pair.first) + bencode(pair.second);
            }
            result += "e";
            return result;
        }
    }
    return "";
}

