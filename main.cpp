#include <iostream>
#include "bencode.h"

int main() {
    std::string test = "d3:cow3:moo4:spam4:eggse";
    size_t index = 0;

    BencodeValue val = parse_any(test, index);

    std::string encoded = bencode(val);

    std::cout << "Original: " << test << "\n";
    std::cout << "Re-encoded: " << encoded << "\n";

    return 0;
}