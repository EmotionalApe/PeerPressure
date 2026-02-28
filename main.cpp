#include <iostream>
#include "bencode.h"

int main() {
    std::string test = "d4:saan5:delhi5:maruf4:punee";
    size_t index = 0;

    BencodeValue val = parse_any(test, index);
    BencodeDict dict = std::get<BencodeDict>(val);

    for (auto& [key, value] : dict) {
       std::visit([&](auto&& v) {
    using T = std::decay_t<decltype(v)>;

    if constexpr (std::is_same_v<T, std::string>) {
        std::cout << key << " : " << v << "\n";
    }
    else if constexpr (std::is_same_v<T, int64_t>) {
        std::cout << key << " : " << v << "\n";
    }
    else {
        std::cout << key << " : [complex type]\n";
    }
}, value);
    }

    return 0;
}