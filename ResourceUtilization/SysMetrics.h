#pragma once
#include <cstdint>

// 系统资源指标结构体
struct SysMetrics {
    // CPU 使用率峰值 (百分比)
    // -1.0 表示初始化失败或未获取到数据
    // >= 0.0 表示有效的峰值百分比
    double cpu_usage_percent_peak = -1.0;

    // 内存相关指标 (单位: KB) - 来自 GloMemPool
    unsigned long long memory_peak_kb = 0;
    unsigned long long memory_current_kb = 0;

    // 内存分配/释放统计 - 来自 GloMemPool
    unsigned long long memory_alloc_count = 0;
    unsigned long long memory_dealloc_count = 0;

    // 当前未释放的内存块数 - 来自 GloMemPool
    long long memory_current_blocks = 0;

    // --- 新增：系统级进程内存指标 (单位: KB) ---
    // 注意：从 Windows API (psapi.h) 获取的通常是字节，需要转换为 KB
    unsigned long long system_pagefile_usage_kb = 0;      // 页文件使用量 (Commit Size)
    unsigned long long system_peak_pagefile_usage_kb = 0; // 峰值页文件使用量 (Peak Commit Size)
    unsigned long long system_working_set_kb = 0;         // 工作集大小 (物理内存中占用的大小)
    unsigned long long system_peak_working_set_kb = 0;    // 峰值工作集大小
    unsigned long long system_private_usage_kb = 0;       // 私有内存使用量 (通常与 Commit Size 相同)
    unsigned long long system_quota_paged_pool_usage_kb = 0;    // 分页池配额使用量
    unsigned long long system_quota_nonpaged_pool_usage_kb = 0; // 非分页池配额使用量
    // --- 新增结束 ---
};