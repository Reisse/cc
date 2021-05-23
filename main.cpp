#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <sys/stat.h>
#include <time.h>

#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <memory>

#include "wiredtiger/wiredtiger.h"

#include "CacheController.h"
#include "WTCache.h"
#include "WTCacheUsageProbe.h"

#define NR_INDICES 16

#define NR_THREADS 12
#define NR_ITER 1000000

#define WT_HOME "put_idx_crash.db"

#define WT_CALL(call) \
    do { \
        const int __rc = call; \
        if (__rc != 0) { \
            fprintf(stderr, # call " at (%s:%d) failed: %s [%d]\n", __FILE__, __LINE__, wiredtiger_strerror(__rc), __rc); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static uint64_t s_base;

static uint64_t get_id()
{
    static uint64_t s_counter = 1;
    return s_base | ++s_counter;
}


static void *
thread_func(void *arg)
{
    WT_CONNECTION *conn;
    WT_SESSION *session;
    WT_CURSOR *cursor;
    WT_ITEM v;
    int i;
    char payload[25];

    v.data = payload;
    v.size = sizeof(payload);

    conn = (WT_CONNECTION *)arg;
    WT_CALL(conn->open_session(conn, NULL, NULL, &session));

    WT_CALL(session->open_cursor(session, "table:main", NULL, NULL, &cursor));

    for (i = 0; i < NR_ITER; ++i) {
        cursor->set_key(cursor, get_id());
        cursor->set_value(cursor, &v,
                          get_id(), get_id(), get_id(), get_id(), 3/*get_id()*/, get_id(), get_id(), get_id(),
                          get_id(), get_id(), get_id(), get_id(), get_id(), get_id(), get_id(), get_id());
        WT_CALL(cursor->insert(cursor));
    }

    WT_CALL(session->close(session, NULL));

    return NULL;
}

int main()
{
    WT_CONNECTION *conn;
    WT_SESSION *session;
    int i, rc;
    char idx_name[16], idx_cfg[32];

    _mkdir(WT_HOME);

    s_base = ((uint64_t) time(NULL)) << 32;

    WT_CALL(wiredtiger_open(WT_HOME, NULL, "create,cache_size=512M,checkpoint=(wait=30),eviction=(threads_max=1),statistics=(fast)", &conn));
    //WT_CALL(wiredtiger_open(WT_HOME, NULL, "create,log=(enabled,file_max=10485760),checkpoint=(wait=30),eviction=(threads_max=1),transaction_sync=(enabled,method=none)", &conn));
    WT_CALL(conn->open_session(conn, NULL, NULL, &session));

    WT_CALL(session->create(session, "table:main", "key_format=Q,value_format=uQQQQQQQQQQQQQQQQ,columns=(k,v,i0,i1,i2,i3,i4,i5,i6,i7,i8,i9,i10,i11,i12,i13,i14,i15)"));  /* NR_INDICES */

    for (i = 0; i < NR_INDICES; ++i) {
        sprintf(idx_name, "index:main:%d", i);
        sprintf(idx_cfg, "columns=(i%d)", i);
        WT_CALL(session->create(session, idx_name, idx_cfg));
    }

    // controller
    /* std::unique_ptr<cc::ICache> cache = std::make_unique<cc::WTCache>(conn, 512ULL*1024ULL*1024ULL);
    std::vector<std::unique_ptr<cc::IProbe>> probes;
    probes.emplace_back(std::make_unique<cc::WTCacheUsageProbe>(session));
    cc::CacheController cacheController{std::move(cache), std::move(probes), cc::ControllerConfig{}}; */
    // controller

    auto t1 = std::chrono::system_clock::now();

    std::vector<std::thread> thrs;
    for (i = 0; i < NR_THREADS; ++i) {
        thrs.emplace_back([&conn] { thread_func(conn); });
    }

    for (auto & thr : thrs) {
        thr.join();
    }

    auto t2 = std::chrono::system_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;

    WT_CALL(conn->close(conn, NULL));

    return 0;
}