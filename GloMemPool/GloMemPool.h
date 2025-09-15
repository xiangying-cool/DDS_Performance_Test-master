// GloMemPool.h
#pragma once
#include <cstddef>
#include <string>

#include "ZRMemPool.h"

#ifdef _MEMORY_USE_TRACK_
#include <unordered_map>
#endif

class GloMemPool {
public:
    struct Stats {
        size_t total_allocated = 0;
        size_t peak_usage = 0;
        size_t alloc_count = 0;
        size_t dealloc_count = 0;
        size_t current_blocks = 0;
    };

    static bool initialize();
    static void finalize();

    static void* allocate(size_t size, const char* file = nullptr, int line = 0);
    static void deallocate(void* ptr);

    // 把模板整个定义放在头文件中（推荐）
    template<typename T, typename... Args>
    static T* new_object(Args&&... args) {
        void* mem = allocate(sizeof(T));
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    template<typename T>
    static void delete_object(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }

    static Stats getStats();

    static bool hasPotentialLeak();
    static size_t getOutstandingAllocations();
    static size_t getCurrentBlocks();

#ifdef _MEMORY_USE_TRACK_
    static size_t getTrackedCount();
#endif

private:
    static ZRMemPool* s_pool;
    static Stats s_stats;

#ifdef _MEMORY_USE_TRACK_
    static std::unordered_map<void*, size_t> s_alloc_map;
#endif

    GloMemPool() = delete;
};