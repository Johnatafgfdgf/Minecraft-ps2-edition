#pragma once
#include <cstdint>

namespace mcps2 {
constexpr uint32_t all_boot_checks = 0x1ffff;
// Small reference vectors and resource checks executed by the console ELF.
// This probe is not the entire parity suite or a hardware performance claim.
uint32_t run_boot_checks();
}
