#include "RDTSC.h"

#include <thread>

namespace cc {

namespace {

static uint64_t rdtsc_rate;
static double rdtsc_rate_nsec;

uint64_t TSC_init_rate() noexcept {
    if (!rdtsc_rate) {
        uint64_t tsc = RDTSC();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        rdtsc_rate = RDTSC() - tsc;

        if (!rdtsc_rate) {
            rdtsc_rate = 1; // for unsupported platforms
        }
        rdtsc_rate_nsec = (double) (rdtsc_rate) / 1000000;
    }
    return rdtsc_rate;
}

} // unnamed namespace

uint64_t TSC_to_usec(uint64_t tsc) noexcept {
    if (!rdtsc_rate) {
        TSC_init_rate();
    }
    // drop high 10 bit to avoid operation overflow
    return ((tsc & 0x3FFFFFFFFFFFFFULL) * 1000) / (rdtsc_rate / 1000);
}

} // namespace cc