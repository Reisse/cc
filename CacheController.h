#pragma once

#include "ControllerConfig.h"
#include "ICache.h"
#include "IProbe.h"

#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace cc {

class CacheController {
public:
    CacheController(
            std::unique_ptr<ICache> cache,
            std::vector<std::unique_ptr<IProbe>> probes,
            ControllerConfig config);
    ~CacheController();

    void thread_fun();

private:
    std::unique_ptr<ICache> m_cache;
    std::vector<std::unique_ptr<IProbe>> m_probes;
    ControllerConfig m_config;

    std::thread m_controller_thread;
    std::promise<void> m_controller_thread_stop_flag;
};

} // namespace cc