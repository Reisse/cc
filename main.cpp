#include "CacheController.h"
#include "Statistics.h"
#include "WTCache.h"
#include "WTCacheUsageProbe.h"

#include "wiredtiger/wiredtiger.h"

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <filesystem>
#include <random>
#include <future>

#define WT_HOME "benchmark.db"

#define WT_CALL(call) \
    do { \
        const int __rc = call; \
        if (__rc != 0) { \
            fprintf(stderr, # call " at (%s:%d) failed: %s [%d]\n", __FILE__, __LINE__, wiredtiger_strerror(__rc), __rc); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

std::string generate_key(uint64_t val) {
    return "aaaadeadfood" + std::to_string(val);
}

size_t generate_payload(char * /* payload */, size_t max_payload_size) {
    static thread_local std::random_device rd{};
    static thread_local std::mt19937 gen{rd()};

    std::normal_distribution<> d{(double) max_payload_size / 2, 1};
    size_t generated_size = (uint64_t) d(gen);
    if (generated_size == 0) { generated_size = 1; }
    if (generated_size > max_payload_size) { generated_size = max_payload_size; }
    return generated_size; // payload is irrelevant
}

void init_database(WT_CONNECTION * conn, uint64_t val_count) {
    WT_SESSION * session;
    WT_CALL(conn->open_session(conn, nullptr, nullptr, &session));

    WT_CURSOR * cursor;
    WT_CALL(session->open_cursor(session, "table:main", nullptr, nullptr, &cursor));

    WT_ITEM k, v;
    char payload[8096];
    v.data = payload;

    for (uint64_t i = 0; i < val_count; ++i) {
        const std::string key = generate_key(i);
        k.data = key.c_str();
        k.size = key.size();
        cursor->set_key(cursor, &k);
        v.size = generate_payload(payload, sizeof(payload));
        cursor->set_value(cursor, &v);
        WT_CALL(cursor->insert(cursor));
    }

    WT_CALL(session->close(session, nullptr));
}

void do_test(WT_CONNECTION * conn, cc::Statistics & stats, uint64_t thr_num, uint64_t val_count, uint64_t iters) {
    static thread_local std::random_device rd{};
    static thread_local std::mt19937 gen{rd()};
    std::uniform_int_distribution<uint64_t> dv{0, val_count - 1};
    std::uniform_int_distribution<uint64_t> dp{0, 1};

    WT_SESSION * session;
    WT_CALL(conn->open_session(conn, nullptr, nullptr, &session));

    WT_CURSOR * cursor;
    WT_CALL(session->open_cursor(session, "table:main", nullptr, nullptr, &cursor));

    WT_ITEM k, v;
    char payload[8096];
    v.data = payload;

    for (uint64_t i = 0; i < iters; ++i) {
        const std::string key = generate_key(dv(gen));
        k.data = key.c_str();
        k.size = key.size();
        cursor->set_key(cursor, &k);
        if (dp(gen)) {
            v.size = generate_payload(payload, sizeof(payload));
            cursor->set_value(cursor, &v);
            stats.thread_in(thr_num, "put");
            WT_CALL(cursor->insert(cursor));
        } else {
            stats.thread_in(thr_num, "get");
            WT_CALL(cursor->search(cursor));
            WT_CALL(cursor->get_value(cursor, &v));
        }
        stats.thread_out(thr_num);
    }

    WT_CALL(session->close(session, nullptr));
}

void cache_logger(std::filesystem::path memory_csv, std::future<void> stop_signal, WT_SESSION * session)
{
    auto probe = std::make_unique<cc::WTCacheUsageProbe>(session);
    std::ofstream stats_file{memory_csv, std::ofstream::out | std::ofstream::app};
    while (stop_signal.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
        stats_file << probe->get_bytes() << std::endl;
    }
}

void memory_hog()
{
    size_t ints_sz = 450ULL * 1000 * 1000;
    int * ints = new int[ints_sz];
    for (size_t i = 0; i < ints_sz; ++i) {
        ints[i] = i;
    }

    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<uint64_t> dv{0, ints_sz - 1};

    // poke pages
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ints[dv(gen)] = dv(gen);
    }
}

int main() {
    void * dummy = malloc(410ULL * 1024 * 1024);

    WT_CONNECTION * conn;
    WT_SESSION * session;

    WT_CALL(wiredtiger_open(WT_HOME, nullptr, "create,cache_size=1536M,checkpoint=(wait=30),eviction=(threads_max=1),statistics=(fast)", &conn));
    WT_CALL(conn->open_session(conn, nullptr, nullptr, &session));
    WT_CALL(session->create(session, "table:main", ""));

    std::queue<std::function<void()>> block_queue;
    std::mutex block_queue_mutex;
    std::condition_variable block_queue_cond;
    bool done = false;

    std::filesystem::path stats_filename = "stats.csv";

    uint64_t threads = 4;
    cc::Statistics stats{threads, threads * 1 * 1024 * 1024, [&] (size_t thread_id, std::unique_ptr<cc::Statistics::ThreadStats> thread_stats) {
        std::unique_lock<std::mutex> lock(block_queue_mutex);
        block_queue.push([&, thread_id, thread_stats = thread_stats.release()] {
            std::ofstream stats_file{stats_filename, std::ofstream::out | std::ofstream::app};
            for (const auto & stat_entry : *thread_stats) {
                stats_file << thread_id << ','
                           << cc::TSC_to_usec(stat_entry.get_rdtsc()) << ','
                           << stat_entry.get_action() << '\n';
            }
            delete thread_stats; // std::function doesn't allow for unique_ptr inside
        });
        block_queue_cond.notify_one();
    }};
    stats.enable();

    std::thread stats_consumer{[&] {
        std::unique_lock<std::mutex> lock(block_queue_mutex);
        while (!done) {
            while (block_queue.empty()) {
                block_queue_cond.wait(lock);
            }

            while (!block_queue.empty()) {
                block_queue.front()();
                block_queue.pop();
            }
        }
    }};

    uint64_t val_count = 1'500'000;
    init_database(conn, val_count);

    // controller
    /* std::unique_ptr<cc::ICache> cache = std::make_unique<cc::WTCache>(conn, 512ULL*1024ULL*1024ULL);
    std::vector<std::unique_ptr<cc::IProbe>> probes;
    probes.emplace_back(std::make_unique<cc::WTCacheUsageProbe>(session));
    cc::CacheController cacheController{std::move(cache), std::move(probes), cc::ControllerConfig{}}; */
    // controller

    std::promise<void> mem_stop_signal;
    std::thread memory_logger{[&] {
        cache_logger("cache.csv", mem_stop_signal.get_future(), session);
    }};

    std::thread mhog(memory_hog);
    mhog.detach();

    uint64_t iters = 500'000;
    std::vector<std::thread> workers;
    for (uint64_t i = 0; i < threads; ++i) {
        workers.emplace_back([&stats, i, conn, val_count, iters] {
            do_test(conn, stats, i, val_count, iters);
        });
    }
    for (auto & worker : workers) {
        worker.join();
    }

    mem_stop_signal.set_value();
    memory_logger.join();

    stats.disable();
    for (size_t i = 0; i < threads; ++i) { // force dump stats
        stats.thread_in(i, "dump");
    }

    {
        std::unique_lock<std::mutex> lock(block_queue_mutex);
        done = true;
    }
    block_queue_cond.notify_one();
    stats_consumer.join();

    WT_CALL(conn->close(conn, nullptr));

    free(dummy);

    return 0;
}
