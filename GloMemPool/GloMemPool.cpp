#include "GloMemPool.h"

ZRMemPool* GloMemPool::s_pool = nullptr;
GloMemPool::Stats GloMemPool::s_stats;

#ifdef _MEMORY_USE_TRACK_
std::unordered_map<void*, size_t> GloMemPool::s_alloc_map;
#endif

bool GloMemPool::initialize() {
    ZRInitialGlobalMemPool();
    s_pool = nullptr;// 默认全局池（可选）
    return true;
}

void GloMemPool::finalize() {
    ZRFinalizeGlobalMemPool();
    s_pool = nullptr;// 清空内存
}

void* GloMemPool::allocate(size_t size, const char* file, int line) {
    void* ptr = nullptr;

#ifdef _MEMORY_USE_TRACK_
    ptr = ZRMallocWCallInfo(s_pool, static_cast<DDS_ULong>(size), file, __FUNCTION__, line);
#else
    ptr = ZRMalloc(s_pool, static_cast<DDS_ULong>(size));
#endif

    if (ptr) {
        s_stats.total_allocated += size;
        s_stats.alloc_count++;
        s_stats.current_blocks++;
        if (s_stats.total_allocated > s_stats.peak_usage) {
            s_stats.peak_usage = s_stats.total_allocated;
        }

#ifdef _MEMORY_USE_TRACK_
        s_alloc_map[ptr] = size;
#endif
    }

    return ptr;
}

void GloMemPool::deallocate(void* ptr) {
    if (!ptr) return;

    ZRDealloc(s_pool, ptr);

#ifdef _MEMORY_USE_TRACK_
    auto it = s_alloc_map.find(ptr);
    if (it != s_alloc_map.end()) {
        size_t size = it->second;
        s_stats.total_allocated -= size;
        s_stats.current_blocks--;
        s_stats.dealloc_count++;
        s_alloc_map.erase(it);
    }
    else {
        s_stats.dealloc_count++;
    }
#else
    s_stats.dealloc_count++;
    s_stats.current_blocks--;
#endif
}

GloMemPool::Stats GloMemPool::getStats() {
    return s_stats;
}

bool GloMemPool::hasPotentialLeak() {
    return s_stats.alloc_count > s_stats.dealloc_count;
}

size_t GloMemPool::getOutstandingAllocations() {
    return s_stats.alloc_count - s_stats.dealloc_count;
}

size_t GloMemPool::getCurrentBlocks() {
    return s_stats.current_blocks;
}

#ifdef _MEMORY_USE_TRACK_
size_t GloMemPool::getTrackedCount() {
    return s_alloc_map.size();
}
#endif

// -------------------------------
// 全局 new/delete 重载（可选）
// -------------------------------

#ifdef ENABLE_GLOBAL_NEW_DELETE

void* operator new(size_t size) {
    void* ptr = GloMemPool::allocate(size, __FILE__, __LINE__);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void operator delete(void* ptr) noexcept {
    GloMemPool::deallocate(ptr);
}

void* operator new[](size_t size) {
    void* ptr = GloMemPool::allocate(size, __FILE__, __LINE__);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void operator delete[](void* ptr) noexcept {
    GloMemPool::deallocate(ptr);
}

#endif