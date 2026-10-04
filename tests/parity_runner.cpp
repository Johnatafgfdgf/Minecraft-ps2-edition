#include "mcps2/random.hpp"
#include "mcps2/position.hpp"
#include "mcps2/bit_storage.hpp"
#include "mcps2/tick_queue.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>

namespace {
unsigned case_index = 0;
uint64_t parse_hex(const std::string& value) { return std::stoull(value, nullptr, 16); }
std::string hex(uint64_t value, unsigned width = 16) {
    std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(width) << value; return out.str();
}
void emit(const std::string& value) { std::cout << "R\t" << case_index << '\t' << value << '\n'; }
template<class R> void rng(uint64_t seed, unsigned count) {
    R source(seed); int32_t result = 0;
    emit(std::string("reject ") + (!source.next_int(0, result) ? '1' : '0'));
    emit(std::string("reject ") + (!source.next_int(-1, result) ? '1' : '0'));
    const int32_t bounds[] = {1,2,3,16,17,1000,1073741825,2147483647};
    for (unsigned i = 0; i < count; ++i) {
        if (i == 127) source.set_seed(seed ^ 0xfedcba9876543210ULL);
        std::string output;
        switch (i % 6) {
            case 0: output = "I " + hex(uint32_t(source.next_int()), 8); break;
            case 1: output = "L " + hex(source.next_long()); break;
            case 2:
                if (!source.next_int(bounds[(i / 6) % 8], result)) std::abort();
                output = "B " + std::to_string(result); break;
            case 3: {
                const float value = source.next_float(); uint32_t bits;
                std::memcpy(&bits, &value, sizeof(bits)); output = "F " + hex(bits, 8); break;
            }
            case 4: {
                const double value = source.next_double(); uint64_t bits;
                std::memcpy(&bits, &value, sizeof(bits)); output = "D " + hex(bits); break;
            }
            default: output = std::string("O ") + (source.next_boolean() ? '1' : '0');
        }
        emit("rng " + std::to_string(i) + " " + output);
    }
}
template<class R> void fork_rng(uint64_t seed, unsigned count) {
    R source(seed);
    for (unsigned i = 0; i < count; ++i) {
        auto child = source.fork();
        const uint64_t child_value = child.next_long(), parent_value = source.next_long();
        emit("fork " + hex(child_value) + " " + hex(parent_value));
    }
}
template<class R> void worldgen(uint64_t seed, int32_t x, int32_t z, int32_t index, int32_t step, int32_t salt) {
    mcps2::WorldgenRandom<R> random(seed);
    const uint64_t derived = random.decoration_seed(seed, x, z);
    emit("decoration " + hex(derived) + " " + hex(random.next_long()));
    random.feature_seed(seed, index, step); emit("feature " + hex(random.next_long()));
    random.large_feature_seed(seed, x, z); emit("large " + hex(random.next_long()));
    random.large_feature_with_salt(seed, x, z, salt);
    const uint64_t value = random.next_long();
    emit("salt " + hex(value) + " " + std::to_string(random.count()));
}
}

int main() {
    using namespace mcps2;
    std::string command;
    while (std::cin >> command) {
        if (command == "rng" || command == "fork") {
            std::string variant, seed; unsigned count; std::cin >> variant >> seed >> count;
            const uint64_t value = parse_hex(seed);
            if (command == "fork") {
                if (variant == "legacy") fork_rng<LegacyRandom>(value, count);
                else fork_rng<XoroshiroRandom>(value, count);
            } else if (variant == "legacy") rng<LegacyRandom>(value, count);
            else if (variant == "xoroshiro") rng<XoroshiroRandom>(value, count);
            else if (variant == "worldgen-legacy") rng<WorldgenRandom<LegacyRandom>>(value, count);
            else if (variant == "worldgen-xoroshiro") rng<WorldgenRandom<XoroshiroRandom>>(value, count);
            else return 2;
        } else if (command == "zero") {
            unsigned count; std::cin >> count; XoroshiroRandom source(0, 0);
            for (unsigned i=0;i<count;++i) emit("zero " + hex(source.next_long()));
        } else if (command == "mix") {
            std::string seed; std::cin >> seed; emit("mix " + hex(mix_stafford13(parse_hex(seed))));
        } else if (command == "pos") {
            int32_t x,y,z; std::cin >> x >> y >> z;
            const uint64_t bp=BlockPos{x,y,z}.pack(), sp=SectionPos{x,y,z}.pack();
            const auto block=BlockPos::unpack(bp); const auto section=SectionPos::unpack(sp);
            emit("block " + hex(bp) + " " + std::to_string(block.x) + " " + std::to_string(block.y) + " " + std::to_string(block.z));
            emit("section " + hex(sp) + " " + std::to_string(section.x) + " " + std::to_string(section.y) + " " + std::to_string(section.z));
            emit("local " + std::to_string(block_to_section(x)) + " " + std::to_string(section_relative({x,y,z})));
            emit("chunk " + hex(chunk_pos(x,z)));
        } else if (command == "worldgen") {
            std::string variant, seed; int32_t x,z,index,step,salt;
            std::cin >> variant >> seed >> x >> z >> index >> step >> salt;
            if (variant == "legacy") worldgen<LegacyRandom>(parse_hex(seed),x,z,index,step,salt);
            else worldgen<XoroshiroRandom>(parse_hex(seed),x,z,index,step,salt);
        } else if (command == "slime") {
            std::string seed,salt; int32_t x,z; std::cin >> seed >> x >> z >> salt;
            auto random=slime_chunk_random(x,z,parse_hex(seed),parse_hex(salt)); int32_t value;
            if (!random.next_int(10,value)) return 3;
            emit("slime " + std::to_string(value) + " " + hex(random.next_long()));
        } else if (command == "storage") {
            unsigned bits; uint32_t entries; std::string seed_text; std::cin >> bits >> entries >> seed_text;
            const uint32_t seed=uint32_t(parse_hex(seed_text));
            const uint32_t mask=bits==32 ? 0x7fffffffu : uint32_t((uint64_t(1)<<bits)-1);
            std::vector<uint64_t> words(BitStorage::required_words(bits,entries)); BitStorage data;
            if (!data.bind(bits,entries,words.data(),words.size())) return 3;
            for (uint32_t i=0;i<entries;++i) if (!data.set(i,(i*0x9e3779b9u+seed)&mask)) return 3;
            uint64_t checksum=0;
            for (int64_t i=int64_t(entries)-1;i>=0;i-=3) {
                uint32_t previous=0;
                if (!data.get_and_set(uint32_t(i),((uint32_t(i)*1664525u+seed)^0x5a5a5a5au)&mask,previous)) return 3;
                checksum ^= uint64_t(previous)*(uint64_t(i)+1);
            }
            emit("previous " + hex(checksum)); checksum=0;
            for (uint32_t i=0;i<entries;++i) {
                uint32_t value=0; if (!data.get(i,value)) return 3;
                checksum=checksum*0x100000001b3ULL ^ value;
            }
            emit("read " + hex(checksum));
            for (size_t i=0;i<words.size();++i) emit("word " + std::to_string(i) + " " + hex(words[i]));
        } else if (command == "ticks") {
            std::string seed_text; unsigned count; std::cin >> seed_text >> count;
            uint32_t seed=uint32_t(parse_hex(seed_text)); std::vector<ScheduledTick> memory(count);
            ChunkTickQueue queue(memory.data(),count); unsigned inserted=0;
            for (unsigned i=0;i<count;++i) {
                seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5;
                const int32_t logical=int32_t(i%97);
                ScheduledTick tick{BlockPos{logical,-64,logical/7}.pack(),int64_t(seed%80)-20,int64_t(i),i%3,int8_t(int(seed%7)-3)};
                const auto result=queue.schedule(tick);
                if (result==ScheduleResult::inserted) ++inserted;
                else if (result!=ScheduleResult::duplicate) return 3;
            }
            emit("inserted " + std::to_string(inserted)); ScheduledTick tick;
            while (queue.poll(tick)) emit("tick " + hex(tick.position) + " " + std::to_string(tick.trigger_tick)
                + " " + std::to_string(tick.priority) + " " + std::to_string(tick.sub_tick_order) + " " + std::to_string(tick.type));
        } else return 2;
        if (!std::cin) return 2;
        ++case_index;
    }
}
