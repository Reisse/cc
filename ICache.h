#pragma once

namespace cc {

class ICache {
public:
    virtual size_t get_cache_size() const = 0;
    virtual void set_cache_size(size_t bytes) = 0;

    virtual ~ICache() = default;
};

} // namespace cc