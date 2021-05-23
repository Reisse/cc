#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace cc {

class ControllerConfig {
public:
    ControllerConfig();
    ~ControllerConfig();

    std::chrono::milliseconds get_check_interval() const;

    std::optional<int64_t> calculate_cache_delta(
            size_t current_cache_size,
            std::string_view probe_name,
            int64_t probe_delta) const;

    std::optional<int64_t> aggregate_cache_deltas(
            std::unordered_map<std::string, int64_t> cache_deltas) const;

private:
    struct ProbeRule {
        enum class Type {
            Absolute,
            Relative
        };

        int64_t probe_step;
        std::variant<int64_t, double> cache_step;
        Type type;
        bool inversion;
    };

private:
    std::chrono::milliseconds m_check_interval;
    std::unordered_map<std::string, ProbeRule> m_probe_rules;
};

} // namespace cc