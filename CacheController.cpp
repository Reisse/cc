#include "CacheController.h"

#include "Log.h"

#include "fmt/core.h"

namespace cc {

CacheController::CacheController(
        std::unique_ptr<ICache> cache,
        std::vector<std::unique_ptr<IProbe>> probes,
        ControllerConfig config) :
    m_cache(std::move(cache)),
    m_probes(std::move(probes)),
    m_config(std::move(config)),
    m_controller_thread([this] { thread_fun(); })
{
    log(LogLevel::Debug, "Cache controller initialized");
}

void CacheController::thread_fun() {
    log(LogLevel::Debug, "Cache controller thread started");

    std::unordered_map<std::string, int64_t> probe_values;

    auto stop_flag = m_controller_thread_stop_flag.get_future();
    while (stop_flag.wait_for(m_config.get_check_interval()) != std::future_status::ready) {
        size_t cache_size = m_cache->get_cache_size();

        std::unordered_map<std::string, int64_t> cache_deltas;
        for (const auto & probe : m_probes) {
            auto probe_name = probe->get_name();
            auto probe_value = probe->get_value();
            log(LogLevel::Debug, fmt::format("Probe \"{}\" returned {}", probe_name, probe_value));

            auto probe_delta = probe_value;
            if (auto it = probe_values.find(std::string(probe_name)); it != probe_values.end()) { // Ugh, waiting for heterogeneous lookup in C++20
                probe_delta = probe_value - it->second;
                it->second = probe_value;
            } else {
                probe_values.emplace(probe_name, probe_value);
            }

            auto cache_delta = m_config.calculate_cache_delta(cache_size, probe_name, probe_delta);
            if (cache_delta) {
                cache_deltas.emplace(probe_name, *cache_delta);
                log(LogLevel::Debug, fmt::format("Calculated delta {} for probe \"{}\"", *cache_delta, probe_name));
            } else {
                log(LogLevel::Warning, fmt::format("No config entry for probe \"{}\"", probe_name));
            }
        }

        if (auto aggr_cache_delta = m_config.aggregate_cache_deltas(cache_deltas)) {
            size_t new_cache_size = cache_size + *aggr_cache_delta;
            m_cache->set_cache_size(new_cache_size);
            log(LogLevel::Debug, fmt::format("New cache size aggregated from deltas: {}", new_cache_size));
        } else {
            log(LogLevel::Warning, fmt::format("Cache deltas not aggregated"));
        }
    }

    log(LogLevel::Debug, "Cache controller thread finished");
}

CacheController::~CacheController() {
    m_controller_thread_stop_flag.set_value();
    m_controller_thread.join();
}

}