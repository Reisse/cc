#pragma once

#pragma once

#include "RDTSC.h"

#include <cassert>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <functional>

#define LIKELY(x) x
#define UNLIKELY(x) x

namespace cc {

class Statistics
{
public:
    class StatEntry
    {
    public:
        struct Out{};

    public:
        StatEntry(uint64_t rdtsc, const char * action) :
            m_rdtsc(rdtsc),
            m_action(action)
        {}

        StatEntry(uint64_t rdtsc, Out) :
            m_rdtsc(rdtsc),
            m_action(reinterpret_cast<const char *>(OutVal))
        {}

        const char * get_action() const {
            if (is_out()) {
                return "<idle>";
            }
            return m_action;
        }

        uint64_t get_rdtsc() const {
            return m_rdtsc;
        }

    private:
        friend class Statistics;

    private:
        static inline constexpr size_t OutVal = 0xEDEDEDED;

        bool is_out() const {
            return m_action == reinterpret_cast<const char *>(OutVal);
        }

    private:
        uint64_t m_rdtsc;
        const char * m_action;
    };

    using ThreadStats = std::vector<StatEntry>;

    using DumpFunc = std::function<void(size_t, std::unique_ptr<ThreadStats>)>;

public:
    Statistics(size_t thread_count, size_t max_stat_count, DumpFunc dump_func) :
        m_thread_count(thread_count),
        m_max_stat_count(max_stat_count),
        m_dump_func(dump_func),
        m_stats(m_thread_count)
    {
    }

    ~Statistics()
    {
        for (size_t i = 0; i < m_thread_count; ++i) {
            if (auto * thread_stats = m_stats[i].exchange(nullptr, std::memory_order_relaxed)) {
                m_dump_func(i, std::unique_ptr<ThreadStats>(thread_stats));
            }
        }
    }

    void enable()
    {
        m_stats_disabled.store(false, std::memory_order_relaxed);
        for (auto & thread_stats : m_stats) {
            ThreadStats * new_thread_stats = new ThreadStats, * expected_thread_stats = nullptr;
            // +1 as wakeup adds two entries, and we want to avoid internal reallocation of vector
            // on any operation
            new_thread_stats->reserve(m_max_stat_count + 1);
            if (!thread_stats.compare_exchange_strong(expected_thread_stats, new_thread_stats, std::memory_order_relaxed)) {
                delete new_thread_stats; // continue with thread stats from previous 'enable'
            }
        }
    }

    void disable()
    {
        m_stats_disabled.store(true, std::memory_order_relaxed);
    }

    void thread_in(size_t thread_num, const char * action)
    {
        if (auto * thread_stats = check_and_get_stats(thread_num); UNLIKELY(thread_stats)) {
            StatEntry & ent = thread_stats->emplace_back(RDTSC(), action);
            tls_thread_action_ptr = &ent.m_action;
        }
    }

    void thread_out(size_t thread_num)
    {
        if (auto * thread_stats = check_and_get_stats(thread_num); UNLIKELY(thread_stats)) {
            tls_thread_action_ptr = &tls_default_thread_action;
            thread_stats->emplace_back(RDTSC(), StatEntry::Out{});
        }
    }

    void thread_wakeup(size_t thread_num, uint64_t rdtsc_notification)
    {
        if (auto * thread_stats = check_and_get_stats(thread_num); UNLIKELY(thread_stats)) {
            thread_stats->emplace_back(rdtsc_notification, "<notification>");
            thread_stats->emplace_back(RDTSC(), "<wakeup>");
        }
    }

    static void set_thread_action(const char * action)
    { *tls_thread_action_ptr = action; }

private:
    ThreadStats * check_and_get_stats(size_t thread_num)
    {
        ThreadStats * thread_stats = m_stats[thread_num].load(std::memory_order_relaxed);
        if (LIKELY(!thread_stats)) { // fastpath
            return nullptr;
        }

        if (UNLIKELY(m_stats_disabled.load(std::memory_order_relaxed))) {
            m_dump_func(thread_num, std::unique_ptr<ThreadStats>(thread_stats));
            tls_thread_action_ptr = &tls_default_thread_action;
            thread_stats = nullptr;
            m_stats[thread_num].store(thread_stats, std::memory_order_relaxed);
        } else if (UNLIKELY(thread_stats->size() >= m_max_stat_count)) {
            m_dump_func(thread_num, std::unique_ptr<ThreadStats>(thread_stats));
            tls_thread_action_ptr = &tls_default_thread_action;
            thread_stats = new ThreadStats;
            // +1 as wakeup adds two entries, and we want to avoid internal reallocation of vector
            // on any operation
            thread_stats->reserve(m_max_stat_count + 1);
            m_stats[thread_num].store(thread_stats, std::memory_order_relaxed);
        }

        return thread_stats;
    }

    size_t m_thread_count = 0, m_max_stat_count = 0;

    DumpFunc m_dump_func;

    std::vector<std::atomic<ThreadStats *>> m_stats;

    std::atomic_bool m_stats_disabled = true;

    static thread_local const char * tls_default_thread_action;
    static thread_local const char ** tls_thread_action_ptr;
};

} // namespace cc