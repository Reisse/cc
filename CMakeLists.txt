cmake_minimum_required(VERSION 3.17)
project(cache_controller)

set(CMAKE_CXX_STANDARD 17)

add_executable(cache_controller main.cpp CacheController.cpp CacheController.h ICache.h IProbe.h ControllerConfig.cpp ControllerConfig.h WTCache.cpp WTCache.h WTCacheUsageProbe.cpp WTCacheUsageProbe.h Log.h Statistics.h RDTSC.cpp RDTSC.h Statistics.cpp)

set(DEPENDENCY_PATHS "wiredtiger/")

find_library(WIREDTIGER NAMES wiredtiger HINTS ${DEPENDENCY_PATHS})
target_link_libraries(cache_controller ${WIREDTIGER})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(cache_controller Threads::Threads)

add_subdirectory(fmt)

find_package(fmt)
target_link_libraries(cache_controller fmt::fmt)
