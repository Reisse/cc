#pragma once

#include <cstdint>
#include <string_view>

namespace cc {

class IProbe {
public:
    virtual std::string_view get_name() const = 0;
    virtual int64_t get_value() const = 0;

    virtual ~IProbe() = default;
};

} // namespace cc