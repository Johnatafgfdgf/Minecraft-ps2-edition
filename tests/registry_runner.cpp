#include "mcps2/block_states.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <iterator>
#include <string>
#include <cassert>

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 2;
    const std::vector<char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    mcps2::BlockStates registry;
    if (!registry.bind(bytes.data(), bytes.size())) return 3;
    // Corrupt/truncated inputs must be rejected before a view becomes usable.
    mcps2::BlockStates bad;
    if (bad.bind(bytes.data(), bytes.size() - 1) || bad.valid()) return 4;
    auto corrupt = bytes; corrupt[4] = 99;
    if (bad.bind(corrupt.data(), corrupt.size()) || bad.valid()) return 4;
    if (registry.block_name(registry.block_count()) || registry.state_block(registry.state_count()) != mcps2::BlockStates::invalid_id) return 4;
    if (argc > 2 && std::string(argv[2]) == "--dump") {
        for (uint32_t i = 0; i < registry.state_count(); ++i) {
            const uint32_t block = registry.state_block(i);
            std::cout << i << '\t' << block << '\t' << registry.block_name(block) << '\t' << (registry.default_state(block) == i ? 1 : 0);
            for (uint32_t p = 0; p < registry.property_count(i); ++p) {
                const char *name, *value;
                if (!registry.property_at(i, p, name, value)) return 4;
                std::cout << '\t' << name << '=' << value;
            }
            std::cout << '\n';
        }
        return 0;
    }
    std::string command, key, value; uint32_t state = 0; unsigned case_index = 0;
    while (std::cin >> command >> state >> key >> value) {
        if (command != "state") return 2;
        uint32_t next = 0;
        if (registry.with_property(state, key.c_str(), value.c_str(), next))
            std::cout << "R\t" << case_index << "\tstate " << next << '\n';
        else std::cout << "R\t" << case_index << "\tstate -1\n";
        ++case_index;
    }
}
