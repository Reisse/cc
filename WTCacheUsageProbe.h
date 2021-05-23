#pragma once

#include "IProbe.h"

#include "wiredtiger/wiredtiger.h"

namespace cc {

class WTCacheUsageProbe : public IProbe {
public:
    WTCacheUsageProbe(WT_SESSION * wt_sess);

    std::string_view get_name() const override;
    int64_t get_value() const override;

private:
    WT_SESSION * m_wt_sess = nullptr;
};

} // namespace cc