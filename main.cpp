#include <iostream>
#include "bencode.h"

int main() {
    std::string test = "d3:cow3:moo4:spam4:eggs4:listl4:saan5:marufi42eee";
    size_t index = 0;

    BencodeValue val = parse_any(test, index);

    print_bencode(val);
    std::cout << "\n";

    return 0;
}