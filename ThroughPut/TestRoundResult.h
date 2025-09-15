#pragma once
#include "SysMetrics.h"

#include <vector>

struct TestRoundResult {
    int round_index;              // 第几轮
    SysMetrics start_metrics;     // 开始时的资源状态
    SysMetrics end_metrics;       // 结束时的资源状态

    // 可选：中间采样点（用于绘制趋势图）
    std::vector<SysMetrics> samples;

    // --- 新增：存储整轮测试的 CPU 使用率历史记录 ---
    // 使用 float 可能比 double 节省一些内存，精度对 CPU % 通常也足够
    std::vector<float> cpu_usage_history;
    // --- 新增结束 ---

    // 注意：如果保留了这个构造函数，需要确保在构造时正确初始化 cpu_usage_history
    // 或者移除它，使用聚合初始化或默认构造函数，然后手动设置字段。
    // 当前这个构造函数没有初始化 samples 和 cpu_usage_history。
    // 为了安全起见，可以添加默认构造函数或使用聚合初始化。
    TestRoundResult() : round_index(0), start_metrics{}, end_metrics{} {
        // vector 成员会自动默认初始化为空
    }

    TestRoundResult(int idx, const SysMetrics& start, const SysMetrics& end)
        : round_index(idx), start_metrics(start), end_metrics(end) {
        // 注意：这个构造函数没有初始化 samples 和 cpu_usage_history vector。
        // 如果使用此构造函数，需要确保在后续填充数据前，vector 是空的或已正确处理。
        // 或者在此处显式初始化：
        // samples = {};
        // cpu_usage_history = {};
    }
};