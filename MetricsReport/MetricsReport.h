// MetricsReport.h
#pragma once

// --- 包含必要的头文件 ---
#include "TestRoundResult.h" // 确保 TestRoundResult 定义（包含 cpu_usage_history）可用
#include <vector>
#include <mutex>
// --- 包含结束 ---

// 资源报告类，用于收集、存储和生成测试轮次的资源使用摘要
class MetricsReport {
public:
    // 添加一轮测试的结果
    void addResult(const TestRoundResult& result);

    // 生成并打印最终的汇总报告
    void generateSummary() const;

private:
    // 存储所有轮次的结果
    std::vector<TestRoundResult> results_;
    // 用于保护 results_ 的互斥锁
    mutable std::mutex mtx_;
};