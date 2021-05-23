#include "WTCacheUsageProbe.h"

#include <stdexcept>

namespace cc {

namespace {

void error_check(int rc, const char * what)
{
    if (rc != 0) {
        throw std::runtime_error(what);
    }
}

int64_t get_stat(WT_CURSOR * cursor, int stat_field)
{
    const char * desc, * pvalue;
    int64_t value;
    cursor->set_key(cursor, stat_field);
    error_check(cursor->search(cursor), "Failed to position cursor");
    error_check(cursor->get_value(cursor, &desc, &pvalue, &value), "Failed to read cursor value");
    return value;
}

} // unnamed namespace

WTCacheUsageProbe::WTCacheUsageProbe(WT_SESSION * wt_sess) :
    m_wt_sess(wt_sess)
{}

std::string_view WTCacheUsageProbe::get_name() const
{
    return "cache_usage_promille";
}

int64_t WTCacheUsageProbe::get_value() const
{
    WT_CURSOR * cursor;
    error_check(
            m_wt_sess->open_cursor(m_wt_sess, "statistics:", nullptr, nullptr, &cursor),
            "Failed to open cursor");
    int64_t cache_size = get_stat(cursor, WT_STAT_CONN_CACHE_BYTES_MAX);
    int64_t cache_use = get_stat(cursor, WT_STAT_CONN_CACHE_BYTES_INUSE);
    return cache_use * 1000 / cache_size;
}

} // namespace cc