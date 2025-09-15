// ResourceUtilization.h
#pragma once

#include "SysMetrics.h" // 确保包含 SysMetrics.h
#include <memory>
#include <vector>

// --- 新增：包含 PerCoreUsage 定义所需的头文件 ---
// 确保在包含 Windows.h 之前包含 cstdint 以获取标准整数类型
// 如果您的项目其他地方已经包含了 Windows.h，请确保包含顺序正确，
// 或者在这里直接包含 Windows.h 来获取 DWORD
#ifdef _WIN32
#include <windows.h> // 包含 DWORD 等 Windows 类型
#else
#include <cstdint>   // 如果非 Windows 平台，使用标准类型
using DWORD = uint32_t; // 简化示例，实际可能需要更精确的映射
#endif
// --- 新增结束 ---

// --- 新增：定义核心使用率结构 ---
struct PerCoreUsage {
    DWORD coreId;        // 核心 ID
    double usagePercent; // 使用率百分比 (-1.0 表示错误)

    // 修复后的默认构造函数
    PerCoreUsage() : coreId(0), usagePercent(-1.0) {}

    // 修复后的带参数构造函数
    PerCoreUsage(DWORD id, double usage) : coreId(id), usagePercent(usage) {}
};
// --- 新增结束 ---

// 资源利用率监控类 (单例)
class ResourceUtilization {
public:
    // 获取单例实例
    static ResourceUtilization& instance();

    // 初始化资源监控 (必须在使用前调用)
    bool initialize();

    // 关闭资源监控 (程序结束前调用)
    void shutdown();

    // 【核心接口】采集当前系统指标
    // 返回值包含自上次调用此方法以来记录到的 CPU 使用率峰值
    SysMetrics collectCurrentMetrics() const;

    // --- 新增：用于控制 CPU 历史记录的方法 ---
    // 启动 CPU 使用率记录
    void start_cpu_recording();

    // 停止 CPU 记录并获取记录的历史数据
    std::vector<float> stop_cpu_recording_and_get_history();
    // --- 新增结束 ---

    // --- 新增：获取每个 CPU 核心使用率的方法 (声明) ---
    bool initializePerCoreMonitoring();
    void shutdownPerCoreMonitoring();
    std::vector<PerCoreUsage> getPerCoreUsageSnapshot() const;
    // --- 新增结束 ---

private:
    // 私有构造/析构函数，防止外部实例化
    ResourceUtilization();
    ~ResourceUtilization();

    // Pimpl (Pointer to Implementation) 模式
    // 隐藏平台相关的实现细节
    class Impl;
    std::unique_ptr<Impl> pimpl_;

    // 标记是否已初始化 (移动到 ResourceUtilization 类中)
    bool is_initialized_ = false;
};

// 注意：SysMetrics.h 必须包含新增的系统级内存指标成员，例如：
/*
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
    unsigned long long memory_alloc_count = 0;
    unsigned long long memory_dealloc_count = 0;
    long long memory_current_blocks = 0;

    // --- 新增：系统级进程内存指标 (单位: KB) ---
    unsigned long long system_pagefile_usage_kb = 0;
    unsigned long long system_peak_pagefile_usage_kb = 0;
    unsigned long long system_working_set_kb = 0;
    unsigned long long system_peak_working_set_kb = 0;
    unsigned long long system_private_usage_kb = 0;
    unsigned long long system_quota_paged_pool_usage_kb = 0;
    unsigned long long system_quota_nonpaged_pool_usage_kb = 0;
    // --- 新增结束 ---
};
*/