#pragma once
#include <cstdint>

namespace mcps2 {
constexpr uint32_t all_boot_checks = 0x1ff;
// Small observed original-JAR vectors, also executed by the native console ELF.
// This probe is not the entire parity suite or a hardware performance claim.
uint32_t run_boot_checks();
}
