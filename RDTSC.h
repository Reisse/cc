#pragma once

#if defined(__GNUC__)
#include <x86intrin.h>
#else
#include <intrin.h>
#endif

#include <cstdint>

namespace cc {

inline uint64_t RDTSC() noexcept {
    return __rdtsc();
}

uint64_t TSC_to_usec(uint64_t tsc) noexcept;

} // namespace cc