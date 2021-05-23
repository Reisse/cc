#pragma once

#include "ICache.h"

#include "wiredtiger/wiredtiger.h"

namespace cc {

class WTCache : public ICache {
public:
    WTCache(WT_CONNECTION * wt_conn, size_t initial_cache_size);

    size_t get_cache_size() const override;
    void set_cache_size(size_t bytes) override;

private:
    WT_CONNECTION * m_wt_conn = nullptr;
    size_t m_cache_size = 0;
};

} // namespace cc