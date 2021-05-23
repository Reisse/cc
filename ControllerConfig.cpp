#include "ControllerConfig.h"

#include <algorithm>
#include <numeric>
#include <cassert>

namespace cc {

ControllerConfig::ControllerConfig()
{
    // For PoC configuration is hardcoded here
    m_check_interval = std::chrono::seconds(1);
    m_probe_rules = {
        { "cache_usage_promille", ProbeRule{ 100, 0.1, ProbeRule::Type::Relative, false } }
    };
}

ControllerConfig::~ControllerConfig() = default;

std::chrono::milliseconds ControllerConfig::get_check_interval() const
{
    return m_check_interval;
}

std::optional<int64_t> ControllerConfig::calculate_cache_delta(
        size_t current_cache_size,
        std::string_view probe_name,
        int64_t probe_delta) const
{
    auto probe_rule_it = m_probe_rules.find(std::string(probe_name)); // Ugh, waiting for heterogeneous lookup in C++20
    if (probe_rule_it == m_probe_rules.end()) {
        return std::nullopt;
    }
    const auto & [_, probe_rule] = *probe_rule_it;

    int64_t cache_step;
    switch (probe_rule.type) {
        case ProbeRule::Type::Absolute:
            cache_step = std::get<int64_t>(probe_rule.cache_step);
            break;
        case ProbeRule::Type::Relative:
            cache_step = std::get<double>(probe_rule.cache_step) * current_cache_size;
            break;
        default:
            return std::nullopt;
    }
    return static_cast<int64_t>(
            ((double) probe_delta / probe_rule.probe_step) * cache_step * (probe_rule.inversion ? -1 : 1));
}

std::optional<int64_t> ControllerConfig::aggregate_cache_deltas(
        std::unordered_map<std::string, int64_t> cache_deltas) const {
    // For PoC only unweighted sum is supported
    return std::accumulate(cache_deltas.begin(), cache_deltas.end(), 0LL,
            [] (int64_t acc, const auto & cache_delta) {
        const auto & [_, delta] = cache_delta;
        return acc + delta;
    });
}

} // namespace cc