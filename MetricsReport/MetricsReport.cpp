// MetricsReport.cpp
#include "MetricsReport.h"
#include "Logger.h"
#include <numeric>
#include <sstream>
#include <iomanip>
#include <algorithm> // for std::max_element
#include <limits>    // for std::numeric_limits (如果需要检查 NaN/Inf)

void MetricsReport::addResult(const TestRoundResult& result) {
    std::lock_guard<std::mutex> lock(mtx_);

    TestRoundResult processed_result = result; // Copy for processing

    // --- 精细化：从历史记录中计算最终的、一致的 CPU 峰值 ---
    if (!result.cpu_usage_history.empty()) {
        // --- 改进：更健壮地查找最大值 ---
        // 使用 std::max_element 找到历史记录中的最大值
        auto max_iter = std::max_element(result.cpu_usage_history.begin(), result.cpu_usage_history.end());

        // 检查迭代器有效性 (理论上 vector 不空时不会失败)
        if (max_iter != result.cpu_usage_history.end()) {
            float final_peak = *max_iter;

            // --- 改进：增加对 NaN 和 Inf 的检查 ---
            // 检查计算出的峰值是否是有效数字
            if (std::isfinite(final_peak)) { // 检查既不是 NaN 也不是 +/- Inf
                // 确保值在合理范围内 (非负)
                if (final_peak >= 0.0f) {
                    // 将计算出的最终峰值存入 processed_result 的 end_metrics 中
                    processed_result.end_metrics.cpu_usage_percent_peak = static_cast<double>(final_peak);
                    Logger::getInstance().logAndPrint(
                        "[MetricsReport::addResult] [PEAK CALCULATED] Round " + std::to_string(result.round_index) +
                        " | Final Peak CPU: " + std::to_string(final_peak) + "% (from history max)"
                    );
                }
                else {
                    // 理论上 GetProcessTimes 不应产生负值，但以防万一
                    Logger::getInstance().logAndPrint(
                        "[MetricsReport::addResult] [WARNING] Round " + std::to_string(result.round_index) +
                        " | Calculated final CPU peak is negative (" + std::to_string(final_peak) + "%). Clamping to 0.0%."
                    );
                    processed_result.end_metrics.cpu_usage_percent_peak = 0.0;
                }
            }
            else {
                // 处理 NaN 或 Inf 的情况
                Logger::getInstance().logAndPrint(
                    "[MetricsReport::addResult] [ERROR] Round " + std::to_string(result.round_index) +
                    " | Calculated final CPU peak is not finite (NaN/Inf: " + std::to_string(final_peak) + "). Setting to -1.0 (Error)."
                );
                processed_result.end_metrics.cpu_usage_percent_peak = -1.0; // 使用 -1.0 标记计算错误
            }
        }
        else {
            // 理论上不会发生 (vector 不空且 max_element 应该能找到元素)
            Logger::getInstance().logAndPrint(
                "[MetricsReport::addResult] [ERROR] Round " + std::to_string(result.round_index) +
                " | cpu_usage_history was not empty but std::max_element failed unexpectedly."
            );
            processed_result.end_metrics.cpu_usage_percent_peak = -1.0; // 标记错误
        }
    }
    else {
        Logger::getInstance().logAndPrint(
            "[MetricsReport::addResult] [INFO] Round " + std::to_string(result.round_index) +
            " | No CPU usage history recorded. Attempting to use internal tracker value or setting to -1.0."
        );
        // 如果没有历史记录，可以尝试使用 result.end_metrics 中可能存在的内部峰值
        // (例如，由 collectCurrentMetrics 提供的，通过 get_cpu_peak_since_last_call 获取的峰值)
        // 如果内部峰值也是 -1.0 或无效，则最终结果保持 -1.0 (无数据)
        double internal_peak = result.end_metrics.cpu_usage_percent_peak;
        if (std::isfinite(internal_peak) && internal_peak >= 0.0) {
            Logger::getInstance().logAndPrint(
                "[MetricsReport::addResult] [PEAK FALLBACK] Round " + std::to_string(result.round_index) +
                " | Using internal tracker peak: " + std::to_string(internal_peak) + "%"
            );
            // processed_result 的 end_metrics.cpu_usage_percent_peak 已经是 internal_peak 了，无需再次赋值
            // 但为了明确，可以再次赋值 (虽然通常不需要)
            // processed_result.end_metrics.cpu_usage_percent_peak = internal_peak;
        }
        else {
            // 内部队列峰值也无效或无数据，保持为 -1.0
            Logger::getInstance().logAndPrint(
                "[MetricsReport::addResult] [INFO] Round " + std::to_string(result.round_index) +
                " | Internal tracker peak is also invalid (" + std::to_string(internal_peak) + ") or not set. Final peak remains -1.0 (No Data/Error)."
            );
            // processed_result.end_metrics.cpu_usage_percent_peak 保持为 -1.0
        }
    }
    // --- 精细化结束 ---

    // 将处理后的结果存入 results_ vector
    results_.push_back(std::move(processed_result));

    // --- 实时输出最终计算出的峰值 ---
    // 注意：这里使用的是处理后的 processed_result
    const auto& end_metrics = processed_result.end_metrics;
    std::ostringstream cpu_oss;
    cpu_oss << std::fixed << std::setprecision(2)
        << "[实时 CPU] 第 " << processed_result.round_index << " 轮 | ";

    // 格式化输出，区分有效值、无数据和错误
    if (end_metrics.cpu_usage_percent_peak >= 0.0) {
        cpu_oss << "CPU峰值: " << end_metrics.cpu_usage_percent_peak << "%";
    }
    else if (end_metrics.cpu_usage_percent_peak == -1.0) {
        cpu_oss << "CPU峰值: 无数据/错误"; // 更明确地表示 -1.0 的含义
    }
    else {
        // 处理其他可能的错误代码 (如果有的话)
        cpu_oss << "CPU峰值: 错误(" << end_metrics.cpu_usage_percent_peak << ")";
    }
    Logger::getInstance().logAndPrint(cpu_oss.str());
    // --- 实时输出结束 ---
}

void MetricsReport::generateSummary() const {
    std::lock_guard<std::mutex> lock(mtx_);

    if (results_.empty()) {
        Logger::getInstance().logAndPrint("[Metrics] 无资源监控数据可汇总");
        return;
    }

    Logger::getInstance().logAndPrint("\n=== 系统资源使用汇总报告 ===");

    for (const auto& r : results_) {
        const auto& start = r.start_metrics;
        const auto& end = r.end_metrics;

        long long net_current_blocks_delta = static_cast<long long>(end.memory_current_blocks) - static_cast<long long>(start.memory_current_blocks);

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2)
            << "第 " << r.round_index << " 轮资源变化 | ";

        // 汇总报告中显示最终计算出的峰值
        if (end.cpu_usage_percent_peak >= 0.0) {
            oss << "CPU峰值: " << end.cpu_usage_percent_peak << "% | ";
        }
        else if (end.cpu_usage_percent_peak == -1.0) {
            oss << "CPU峰值: 无数据/错误 | ";
        }
        else {
            oss << "CPU峰值: 错误(" << end.cpu_usage_percent_peak << ") | ";
        }

        // GloMemPool 内存信息
        oss << "GloMemPool内存增量: " << (end.memory_current_kb - start.memory_current_kb) << " KB | "
            << "GloMemPool峰值内存: " << end.memory_peak_kb << " KB | "
            << "GloMemPool净分配块数 (当前块): " << net_current_blocks_delta << " | ";

        // --- 修改：安全地计算和打印系统级内存增量 ---
        // 关键：使用 unsigned long long 进行减法，然后处理可能的“负”结果

        // 1. 工作集增量 (Working Set)
        unsigned long long working_set_start = start.system_working_set_kb;
        unsigned long long working_set_end = end.system_working_set_kb;
        long long working_set_delta_kb = 0;
        if (working_set_end >= working_set_start) {
            working_set_delta_kb = static_cast<long long>(working_set_end - working_set_start);
        }
        else {
            // 处理 end < start 的情况（虽然不常见，但可能发生）
            // 方式1：简单地设为0 (推荐，避免显示巨大的负数)
            working_set_delta_kb = 0;
            // 方式2：计算负值（需要确保打印时使用 %lld）
            // working_set_delta_kb = -static_cast<long long>(working_set_start - working_set_end);
            // Logger::getInstance().logAndPrint("[MetricsReport] Warning: Working set end < start in round " + std::to_string(r.round_index));
        }
        oss << "系统工作集增量: " << working_set_delta_kb << " KB | ";

        // 2. 峰值工作集 (Peak Working Set) - 通常只报告最终值
        oss << "系统峰值工作集: " << end.system_peak_working_set_kb << " KB | ";

        // 3. 页文件使用增量 (Pagefile Usage / Commit Size)
        unsigned long long pagefile_start = start.system_pagefile_usage_kb;
        unsigned long long pagefile_end = end.system_pagefile_usage_kb;
        long long pagefile_delta_kb = 0;
        if (pagefile_end >= pagefile_start) {
            pagefile_delta_kb = static_cast<long long>(pagefile_end - pagefile_start);
        }
        else {
            // 处理 end < start 的情况
            // pagefile_delta_kb = 0; // 方式1 (推荐)
            // pagefile_delta_kb = -static_cast<long long>(pagefile_start - pagefile_end); // 方式2
            // Logger::getInstance().logAndPrint("[MetricsReport] Warning: Pagefile usage end < start in round " + std::to_string(r.round_index));
            pagefile_delta_kb = 0; // 推荐方式
        }
        oss << "系统页文件增量: " << pagefile_delta_kb << " KB";
        // --- 修改结束 ---


        Logger::getInstance().logAndPrint(oss.str());
    }
}