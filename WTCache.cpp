#include "WTCache.h"

#include "fmt/core.h"

#include <stdexcept>

namespace cc {

WTCache::WTCache(WT_CONNECTION * wt_conn, size_t initial_cache_size) :
    m_wt_conn(wt_conn),
    m_cache_size(initial_cache_size)
{}

size_t WTCache::get_cache_size() const
{
    return m_cache_size;
}

void WTCache::set_cache_size(size_t bytes)
{
    std::string new_conf = fmt::format("cache_size={}", bytes);
    int rc = m_wt_conn->reconfigure(m_wt_conn, new_conf.c_str());
    if (rc != 0) {
        throw std::runtime_error("Failed to reconfigure cache");
    }
    m_cache_size = bytes;
}

} // namespace cc