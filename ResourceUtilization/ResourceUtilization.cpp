#include "ResourceUtilization.h"
#include "GloMemPool.h" // 用于获取内存 stats
#include "Logger.h"     // 用于输出调试日志

// Windows 平台特定头文件
#ifdef _WIN32
#include <windows.h>
// --- 新增：包含 psapi.h 以使用 GetProcessMemoryInfo ---
#include <psapi.h> // 包含 GetProcessMemoryInfo 所需的头文件
#pragma comment(lib, "psapi.lib") // 链接 psapi.lib 库
// --- 新增结束 ---
#include <sstream>      // 用于格式化错误信息
#include <chrono>       // 用于时间间隔控制
#include <thread>       // 用于后台采样线程
#include <atomic>       // 用于线程安全的峰值存储
#include <algorithm>    // 用于 std::max
#include <string>       // for std::string, needed for WideCharToMultiByte conversion
// --- 新增：PDH 头文件 ---
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
// --- 新增结束 ---
#endif

#include <vector> // 确保包含 vector

// -----------------------------
// Implementation (Pimpl)
// -----------------------------

// 内部实现类，封装了具体的监控逻辑
class ResourceUtilization::Impl {
public:
    // 构造函数：初始化成员变量
    Impl() : is_initialized_(false), pid_(0), sampling_thread_(nullptr),
        stop_sampling_(false), current_cpu_peak_(-1.0)
#ifdef _WIN32
        , query_(nullptr) // 初始化 PDH 查询句柄
#endif
    {
#ifdef _WIN32
        process_handle_ = NULL;
#endif
    }

    // 析构函数：确保资源被清理
    ~Impl() {
        shutdown_internal(); // 确保在析构时清理所有资源，包括 PDH
    }

    // 内部初始化函数
    bool initialize_internal() {
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Starting initialization (Peak GetProcessTimes method)...");

        // 如果已经初始化，直接返回成功
        if (is_initialized_) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Already initialized.");
            return true;
        }

#ifdef _WIN32
        // 获取当前进程 ID
        pid_ = GetCurrentProcessId();
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Current Process ID: " + std::to_string(pid_));

        // 获取当前进程句柄 (伪句柄，无需 CloseHandle)
        process_handle_ = GetCurrentProcess();
        if (process_handle_ == NULL) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Error: Failed to get current process handle.");
            return false;
        }
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Got process handle.");

        // 初始化时获取一次时间，检查 API 可用性
        FILETIME dummy_ft1, dummy_ft2, dummy_ft3, dummy_ft4;
        if (!GetSystemTimes(&dummy_ft1, &dummy_ft2, &dummy_ft3) ||
            !GetProcessTimes(process_handle_, &dummy_ft1, &dummy_ft2, &dummy_ft3, &dummy_ft4)) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Error: Initial GetSystemTimes or GetProcessTimes failed.");
            process_handle_ = NULL;
            return false;
        }

        // --- 启动后台采样线程 ---
        stop_sampling_ = false;
        current_cpu_peak_ = -1.0; // 重置峰值
        sampling_thread_ = new std::thread(&Impl::sampling_loop, this);
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Sampling thread started.");
        // --- 启动结束 ---

        // --- 新增：初始化每个核心的监控 ---
        if (!initialize_per_core_internal()) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Warning: Per-core monitoring initialization failed or not supported.");
            // 可以选择让整体初始化失败，或者继续 (取决于需求)
            // return false; // 如果核心监控是必须的，可以在这里返回 false
        }
        else {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Per-core monitoring initialized.");
        }
        // --- 新增结束 ---

        is_initialized_ = true;
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Successfully initialized (Peak GetProcessTimes method).");
        return true;

#else
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_internal] Error: GetProcessTimes method not supported on non-Windows.");
        return false;
#endif
    }

    // 内部关闭函数
    void shutdown_internal() {
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::shutdown_internal] Shutting down...");

        // --- 停止并等待后台采样线程 ---
        if (sampling_thread_ && sampling_thread_->joinable()) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::shutdown_internal] Stopping sampling thread...");
            stop_sampling_ = true;
            sampling_thread_->join();
            delete sampling_thread_;
            sampling_thread_ = nullptr;
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::shutdown_internal] Sampling thread stopped and joined.");
        }
        // --- 停止结束 ---

        // --- 新增：关闭每个核心的监控 ---
        shutdown_per_core_internal();
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::shutdown_internal] Per-core monitoring shut down.");
        // --- 新增结束 ---

        is_initialized_ = false;
        pid_ = 0;
        current_cpu_peak_ = -1.0; // 重置峰值
#ifdef _WIN32
        process_handle_ = NULL;
#endif
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::shutdown_internal] Shutdown complete.");
    }

    // --- 新增：内部初始化每个核心监控 ---
    bool initialize_per_core_internal() {
#ifdef _WIN32
        if (query_) { // 如果已经初始化过，先关闭
            PdhCloseQuery(query_);
            query_ = nullptr;
        }
        counters_.clear();
        counterPaths_.clear(); // Assume counterPaths_ stores std::wstring

        PDH_STATUS status = PdhOpenQuery(NULL, 0, &query_);
        if (status != ERROR_SUCCESS) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_per_core_internal] Error opening PDH query: " + std::to_string(status));
            return false;
        }

        // --- 动态获取 CPU 核心数量和路径 ---
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        DWORD numberOfProcessors = sysInfo.dwNumberOfProcessors;

        // 尝试较新的路径格式 (Processor Information)
        counterPaths_.clear();
        for (DWORD i = 0; i < numberOfProcessors; ++i) {
            // Processor Information 格式: \Processor Information(<Group>,<Number>)\% Processor Time
            // 简化起见，假设 Group 0 (对于大多数系统)
            std::wstring path = L"\\Processor Information(0," + std::to_wstring(i) + L")\\% Processor Time";
            counterPaths_.push_back(path);
        }

        bool fallbackNeeded = false;
        for (size_t i = 0; i < counterPaths_.size(); ++i) {
            const auto& path = counterPaths_[i];
            PDH_HCOUNTER counter;
            status = PdhAddCounter(query_, path.c_str(), 0, &counter);
            if (status != ERROR_SUCCESS) {
                Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_per_core_internal] Error adding counter (" + std::to_string(i) + " - " + wstring_to_string(path) + "): " + std::to_string(status));
                fallbackNeeded = true;
                break; // 如果一个失败，可能需要全部回退
            }
            else {
                counters_.push_back(counter);
            }
        }

        // 如果 Processor Information 路径失败，回退到 \Processor(<Number>)\% Processor Time
        if (fallbackNeeded) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_per_core_internal] Falling back to \\Processor(N)\\% Processor Time format.");
            // 清理已添加的计数器和路径
            for (auto c : counters_) PdhRemoveCounter(c);
            counters_.clear();
            counterPaths_.clear();

            for (DWORD j = 0; j < numberOfProcessors; ++j) {
                std::wstring fallbackPath = L"\\Processor(" + std::to_wstring(j) + L")\\% Processor Time";
                counterPaths_.push_back(fallbackPath);
            }

            for (size_t k = 0; k < counterPaths_.size(); ++k) {
                const auto& fallbackPath = counterPaths_[k];
                PDH_HCOUNTER fallbackCounter;
                status = PdhAddCounter(query_, fallbackPath.c_str(), 0, &fallbackCounter);
                if (status == ERROR_SUCCESS) {
                    counters_.push_back(fallbackCounter);
                }
                else {
                    Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_per_core_internal] Error adding fallback counter (" + std::to_string(k) + " - " + wstring_to_string(fallbackPath) + "): " + std::to_string(status));
                    // 可以选择失败或跳过该核心，这里选择继续尝试
                }
            }
        }

        if (counters_.empty()) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_per_core_internal] No counters could be added.");
            PdhCloseQuery(query_);
            query_ = nullptr;
            return false;
        }

        // 执行一次初始查询以建立基线
        status = PdhCollectQueryData(query_);
        if (status != ERROR_SUCCESS) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_per_core_internal] Warning: Error collecting initial PDH  " + std::to_string(status));
            // 不一定导致初始化失败，但后续第一次查询可能不准
        }

        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_per_core_internal] Successfully initialized per-core monitoring for " + std::to_string(counters_.size()) + " cores.");
        return true;
#else
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::initialize_per_core_internal] Per-core monitoring not implemented for non-Windows.");
        return false;
#endif
    }
    // --- 新增结束 ---

    // --- 新增：内部关闭每个核心监控 ---
    void shutdown_per_core_internal() {
#ifdef _WIN32
        if (query_) {
            PdhCloseQuery(query_);
            query_ = nullptr;
        }
        counters_.clear();
        counterPaths_.clear();
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::shutdown_per_core_internal] Per-core monitoring resources released.");
#endif
    }
    // --- 新增结束 ---

    // --- 新增：获取每个核心使用率快照 ---
    std::vector<PerCoreUsage> get_per_core_usage_snapshot_internal() const {
        std::vector<PerCoreUsage> coreUsages;
#ifdef _WIN32
        if (!query_) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::get_per_core_usage_snapshot_internal] Error: PDH query not initialized.");
            return coreUsages; // 返回空
        }

        PDH_STATUS status = PdhCollectQueryData(query_);
        if (status != ERROR_SUCCESS) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::get_per_core_usage_snapshot_internal] Error collecting PDH  " + std::to_string(status));
            // 可以选择返回空或添加错误标记的条目
            // return coreUsages;
        }

        // 注意：为了获得有意义的瞬时值，两次 PdhCollectQueryData 之间需要有一定时间间隔。
        // 这里的实现假设调用者控制了采样频率。
        // 如果需要内部控制，可以在 PdhCollectQueryData 之间 Sleep，但这会阻塞调用线程。

        for (size_t i = 0; i < counters_.size(); ++i) {
            PDH_FMT_COUNTERVALUE counterVal;
            // 使用 PDH_FMT_DOUBLE 来获取浮点数格式的值
            status = PdhGetFormattedCounterValue(counters_[i], PDH_FMT_DOUBLE, NULL, &counterVal);
            if (status == ERROR_SUCCESS) {
                DWORD coreId = static_cast<DWORD>(i); // 默认使用索引
                // 从路径中提取核心 ID (简化处理)
                if (i < counterPaths_.size()) {
                    const std::wstring& path = counterPaths_[i];
                    size_t start = path.find_last_of(L"(");
                    size_t end = path.find_last_of(L")");
                    if (start != std::wstring::npos && end != std::wstring::npos && start < end) {
                        std::wstring coreIdStr = path.substr(start + 1, end - start - 1);
                        // 处理 Processor Information(0,X) 的情况，只取逗号后的 X
                        size_t comma = coreIdStr.find(L",");
                        if (comma != std::wstring::npos) {
                            coreIdStr = coreIdStr.substr(comma + 1);
                        }
                        try {
                            coreId = static_cast<DWORD>(std::stoi(coreIdStr));
                        }
                        catch (...) {
                            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::get_per_core_usage_snapshot_internal] Warning: Could not parse core ID from path, using index " + std::to_string(i));
                            coreId = static_cast<DWORD>(i); // Fallback to index
                        }
                    }
                }
                coreUsages.emplace_back(coreId, counterVal.doubleValue);
            }
            else {
                Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::get_per_core_usage_snapshot_internal] Error getting formatted counter value for counter " + std::to_string(i) + ": " + std::to_string(status));
                // 添加一个表示错误的 CoreUsage 条目
                DWORD errorCoreId = (i < counterPaths_.size()) ? static_cast<DWORD>(i) : static_cast<DWORD>(i); // 尽量提供 ID
                coreUsages.emplace_back(errorCoreId, -1.0); // 用 -1.0 表示错误
            }
        }
#endif
        return coreUsages;
    }
    // --- 新增结束 ---


    // --- 新增：后台采样循环 ---
    // 在独立线程中高频采样 CPU 使用率并更新峰值
    void sampling_loop() {
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::sampling_loop] Sampling thread started loop.");
        // 定义采样间隔 (例如，每 20ms 采样一次)
        const std::chrono::milliseconds sampling_interval(20);

        FILETIME prev_sys_idle, prev_sys_kernel, prev_sys_user;
        FILETIME prev_proc_creation, prev_proc_exit, prev_proc_kernel, prev_proc_user;

        // 获取初始时间点
        if (!GetSystemTimes(&prev_sys_idle, &prev_sys_kernel, &prev_sys_user) ||
            !GetProcessTimes(process_handle_, &prev_proc_creation, &prev_proc_exit, &prev_proc_kernel, &prev_proc_user)) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::sampling_loop] Error: Initial GetSystemTimes or GetProcessTimes failed in sampling loop.");
            return;
        }

        // 持续采样循环
        while (!stop_sampling_) {
            // 等待指定的采样间隔
            std::this_thread::sleep_for(sampling_interval);

            FILETIME sys_idle, sys_kernel, sys_user;
            FILETIME proc_creation, proc_exit, proc_kernel, proc_user;

            // 获取当前时间点的系统和进程时间
            if (!GetSystemTimes(&sys_idle, &sys_kernel, &sys_user) ||
                !GetProcessTimes(process_handle_, &proc_creation, &proc_exit, &proc_kernel, &proc_user)) {
                Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::sampling_loop] Error: GetSystemTimes or GetProcessTimes failed during sampling.");
                continue; // 跳过本次循环，继续下次尝试
            }

            // 将 FILETIME 转换为 64 位整数 (100-nanosecond intervals)
            ULARGE_INTEGER sys_kernel1, sys_user1, sys_kernel2, sys_user2;
            ULARGE_INTEGER proc_kernel1, proc_user1, proc_kernel2, proc_user2;

            sys_kernel1.LowPart = prev_sys_kernel.dwLowDateTime; sys_kernel1.HighPart = prev_sys_kernel.dwHighDateTime;
            sys_user1.LowPart = prev_sys_user.dwLowDateTime; sys_user1.HighPart = prev_sys_user.dwHighDateTime;
            sys_kernel2.LowPart = sys_kernel.dwLowDateTime; sys_kernel2.HighPart = sys_kernel.dwHighDateTime;
            sys_user2.LowPart = sys_user.dwLowDateTime; sys_user2.HighPart = sys_user.dwHighDateTime;

            proc_kernel1.LowPart = prev_proc_kernel.dwLowDateTime; proc_kernel1.HighPart = prev_proc_kernel.dwHighDateTime;
            proc_user1.LowPart = prev_proc_user.dwLowDateTime; proc_user1.HighPart = prev_proc_user.dwHighDateTime;
            proc_kernel2.LowPart = proc_kernel.dwLowDateTime; proc_kernel2.HighPart = proc_kernel.dwHighDateTime;
            proc_user2.LowPart = proc_user.dwLowDateTime; proc_user2.HighPart = proc_user.dwHighDateTime;

            // 计算系统和进程的时间差
            ULONGLONG sys_kernel_delta = sys_kernel2.QuadPart - sys_kernel1.QuadPart;
            ULONGLONG sys_user_delta = sys_user2.QuadPart - sys_user1.QuadPart;
            ULONGLONG sys_total_delta = sys_kernel_delta + sys_user_delta;

            ULONGLONG proc_kernel_delta = proc_kernel2.QuadPart - proc_kernel1.QuadPart;
            ULONGLONG proc_user_delta = proc_user2.QuadPart - proc_user1.QuadPart;
            ULONGLONG proc_total_delta = proc_kernel_delta + proc_user_delta;

            // 计算 CPU 使用率百分比
            double cpu_usage = 0.0;
            if (sys_total_delta != 0) {
                cpu_usage = (static_cast<double>(proc_total_delta) / static_cast<double>(sys_total_delta)) * 100.0;
            }
            // 确保结果非负
            if (cpu_usage < 0.0) cpu_usage = 0.0;

            // --- 原子地更新峰值 ---
            // 使用 std::atomic<double> 的 compare_exchange_weak 来实现无锁更新
            double current_peak = current_cpu_peak_.load();
            while (cpu_usage > current_peak) {
                // 如果当前计算出的 cpu_usage 高于已记录的峰值 current_peak，
                // 则尝试用 cpu_usage 更新 current_cpu_peak_。
                // compare_exchange_weak 会自动处理并发情况：
                // - 如果 current_cpu_peak_ 的值仍然是 current_peak，则更新为 cpu_usage，并返回 true。
                // - 如果 current_cpu_peak_ 的值已经被其他线程修改（不再是 current_peak），则返回 false，
                //   并将 current_peak 更新为 current_cpu_peak_ 的最新值，然后循环继续尝试。
                if (current_cpu_peak_.compare_exchange_weak(current_peak, cpu_usage)) {
                    // 成功更新峰值
                    Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::sampling_loop] New peak CPU usage: " + std::to_string(cpu_usage) + "%");
                    break; // 跳出循环
                }
                // 如果 compare_exchange_weak 失败，current_peak 会被自动更新为最新的值，然后循环继续尝试
            }
            // --- 更新结束 ---

            // 更新 previous times 供下次迭代使用
            prev_sys_idle = sys_idle;
            prev_sys_kernel = sys_kernel;
            prev_sys_user = sys_user;
            prev_proc_creation = proc_creation;
            prev_proc_exit = proc_exit;
            prev_proc_kernel = proc_kernel;
            prev_proc_user = proc_user;

        } // while (!stop_sampling_)
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::sampling_loop] Sampling thread exiting loop.");
    }
    // --- 新增结束 ---

    // --- 修改：get_cpu_peak_since_last_call 现在只返回并重置峰值 ---
    // 这个函数由 collectCurrentMetrics 调用，获取并重置由后台线程维护的峰值
    double get_cpu_peak_since_last_call() {
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::get_cpu_peak_since_last_call] Called.");
        if (!is_initialized_) {
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::get_cpu_peak_since_last_call] Error: Not initialized.");
            return -1.0;
        }
        // 原子地加载当前峰值，并将其重置为 -1.0 (表示下一轮监控周期的开始)
        // exchange 操作是原子的：它返回旧值，并将新值存入 atomic 变量
        double peak = current_cpu_peak_.exchange(-1.0);
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::get_cpu_peak_since_last_call] Returning peak: " + std::to_string(peak) + "% and resetting internal peak tracker.");
        return peak;
    }
    // --- 修改结束 ---

    // --- 新增：用于收集系统内存信息的私有方法声明 ---
    void collect_system_memory_info(SysMetrics& metrics_out) const;
    // --- 新增结束 ---

    // --- 新增：辅助函数：安全地将 std::wstring 转换为 std::string ---
    // 解决 C4244 警告
    static std::string wstring_to_string(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        if (size_needed <= 0) return std::string(); // Handle conversion error
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
        return str;
    }
    // --- 新增结束 ---

private:
    // --- 成员变量 ---
    bool is_initialized_;           // 是否已初始化
    mutable DWORD pid_;             // 当前进程 ID
#ifdef _WIN32
    mutable HANDLE process_handle_; // 当前进程句柄 (伪句柄)
#endif

    // 后台采样相关
    std::thread* sampling_thread_;  // 后台采样线程指针
    std::atomic<bool> stop_sampling_; // 停止采样的标志
    std::atomic<double> current_cpu_peak_; // 存储当前采样周期内的 CPU 使用率峰值

    // --- 新增：每个核心监控相关 ---
#ifdef _WIN32
    mutable PDH_HQUERY query_; // PDH 查询句柄
    mutable std::vector<PDH_HCOUNTER> counters_; // PDH 计数器句柄列表
    mutable std::vector<std::wstring> counterPaths_; // 存储每个核心的计数器路径 (wstring)
#endif
    // --- 新增结束 ---
    // --- 成员变量结束 ---
};

// -----------------------------
// Public Interface (公共接口实现)
// -----------------------------

// 获取单例实例
ResourceUtilization& ResourceUtilization::instance() {
    static ResourceUtilization inst;
    return inst;
}

// 构造函数
ResourceUtilization::ResourceUtilization()
    : pimpl_(std::make_unique<Impl>()), is_initialized_(false) {
}

// 析构函数
ResourceUtilization::~ResourceUtilization() {
    shutdown(); // 确保在析构时清理资源
}

// 公共初始化接口
bool ResourceUtilization::initialize() {
    Logger::getInstance().logAndPrint("[ResourceUtilization::initialize] Requested initialization.");
    bool ok = pimpl_->initialize_internal();
    if (ok) {
        is_initialized_ = true;
        Logger::getInstance().logAndPrint("[ResourceUtilization::initialize] Initialization successful.");
    }
    else {
        is_initialized_ = false;
        Logger::getInstance().logAndPrint("[ResourceUtilization::initialize] Initialization failed.");
    }
    return ok;
}

// 公共关闭接口
void ResourceUtilization::shutdown() {
    Logger::getInstance().logAndPrint("[ResourceUtilization::shutdown] Requested shutdown.");
    if (pimpl_) {
        pimpl_->shutdown_internal();
    }
    is_initialized_ = false;
    Logger::getInstance().logAndPrint("[ResourceUtilization::shutdown] Shutdown process completed.");
}

// 【核心】采集当前系统指标
SysMetrics ResourceUtilization::collectCurrentMetrics() const {
    Logger::getInstance().logAndPrint("[ResourceUtilization::collectCurrentMetrics] Collecting metrics...");
    SysMetrics metrics{};

    // 初始化所有指标为默认值 (包括新增的系统级指标)
    metrics.cpu_usage_percent_peak = -1.0;
    metrics.memory_peak_kb = 0;
    metrics.memory_current_kb = 0;
    metrics.memory_alloc_count = 0;
    metrics.memory_dealloc_count = 0;
    metrics.memory_current_blocks = 0;
    // --- 新增：初始化系统级内存指标 ---
    metrics.system_pagefile_usage_kb = 0;
    metrics.system_peak_pagefile_usage_kb = 0;
    metrics.system_working_set_kb = 0;
    metrics.system_peak_working_set_kb = 0;
    metrics.system_private_usage_kb = 0;
    metrics.system_quota_paged_pool_usage_kb = 0;
    metrics.system_quota_nonpaged_pool_usage_kb = 0;
    // --- 新增结束 ---

    // 1. CPU 使用率峰值 (保持原有逻辑不变)
    // --- 修改：调用新的峰值获取函数 ---
    double cpu_peak = pimpl_->get_cpu_peak_since_last_call();
    // --- 修改结束 ---
    if (cpu_peak >= 0.0) {
        metrics.cpu_usage_percent_peak = cpu_peak;
        Logger::getInstance().logAndPrint("[ResourceUtilization::collectCurrentMetrics] CPU usage peak (valid): " + std::to_string(cpu_peak));
    }
    else if (cpu_peak == -1.0) {
        // 这可能意味着初始化失败，或者在采样周期内没有获取到有效数据 (例如，进程非常空闲)
        metrics.cpu_usage_percent_peak = -1.0;
        Logger::getInstance().logAndPrint("[ResourceUtilization::collectCurrentMetrics] CPU usage peak collection returned -1.0 (no data or error).");
    }
    // 注意：由于后台线程持续运行，理论上不太可能返回 < -1.0 的值

    // 2. 内存统计来自 GloMemPool (保持原有逻辑不变)
    Logger::getInstance().logAndPrint("[ResourceUtilization::collectCurrentMetrics] Collecting memory stats from GloMemPool...");
    auto mem_stats = GloMemPool::getStats();
    metrics.memory_peak_kb = static_cast<unsigned long long>(mem_stats.peak_usage / 1024);
    metrics.memory_current_kb = static_cast<unsigned long long>(mem_stats.total_allocated / 1024);
    metrics.memory_alloc_count = mem_stats.alloc_count;
    metrics.memory_dealloc_count = mem_stats.dealloc_count;
    metrics.memory_current_blocks = mem_stats.current_blocks;
    Logger::getInstance().logAndPrint("[ResourceUtilization::collectCurrentMetrics] GloMemPool stats collected.");

    // --- 新增：委托给 Impl 收集系统级进程内存信息 ---
    // 通过 Impl 的私有方法安全地访问其成员并收集系统内存信息
    pimpl_->collect_system_memory_info(metrics);
    // --- 新增结束 ---

    Logger::getInstance().logAndPrint("[ResourceUtilization::collectCurrentMetrics] Metrics collection complete.");
    return metrics;
}


// --- 新增：公共接口实现 ---
bool ResourceUtilization::initializePerCoreMonitoring() {
    if (!is_initialized_ || !pimpl_) {
        Logger::getInstance().logAndPrint("[ResourceUtilization::initializePerCoreMonitoring] Error: Main resource utilization not initialized.");
        return false;
    }
    // 委托给 Impl 的内部初始化方法
    return pimpl_->initialize_per_core_internal();
}

void ResourceUtilization::shutdownPerCoreMonitoring() {
    if (pimpl_) {
        // 委托给 Impl 的内部关闭方法
        pimpl_->shutdown_per_core_internal();
    }
}

std::vector<PerCoreUsage> ResourceUtilization::getPerCoreUsageSnapshot() const {
    if (!is_initialized_ || !pimpl_) {
        Logger::getInstance().logAndPrint("[ResourceUtilization::getPerCoreUsageSnapshot] Error: ResourceUtilization not initialized.");
        return {}; // Return empty vector if not initialized
    }
    // 委托给 Impl 的内部获取快照方法
    return pimpl_->get_per_core_usage_snapshot_internal();
}
// --- 新增结束 ---

// --- 其他现有方法 (start_cpu_recording, stop_cpu_recording_and_get_history) 的实现 ---
// (请将您原有的实现放在这里，并确保 stop_cpu_recording_and_get_history 有 return 语句)
void ResourceUtilization::start_cpu_recording() {
    // ... (您的现有实现逻辑) ...
    // TODO: 实现 CPU 记录启动逻辑
    Logger::getInstance().logAndPrint("[ResourceUtilization::start_cpu_recording] CPU recording started (placeholder).");
}

std::vector<float> ResourceUtilization::stop_cpu_recording_and_get_history() {
    // ... (您的现有实现逻辑) ...
    // 例如：
    std::vector<float> history;
    // ... (填充 history 的逻辑) ...
    // ... (可能的清理逻辑) ...
    Logger::getInstance().logAndPrint("[ResourceUtilization::stop_cpu_recording_and_get_history] CPU recording stopped and history retrieved (placeholder).");
    return history; // <--- 确保有 return 语句
    // 如果暂时没有实现，可以返回空 vector:
    // return {};
}
// --- 其他现有方法结束 ---


// --- 新增：Impl 类中 collect_system_memory_info 方法的实现 ---
void ResourceUtilization::Impl::collect_system_memory_info(SysMetrics& metrics_out) const {
#ifdef _WIN32
    // 现在可以在 Impl 内部安全地访问自己的私有成员 is_initialized_ 和 process_handle_
    if (this->is_initialized_ && this->process_handle_) {
        PROCESS_MEMORY_COUNTERS_EX pmc_ex = { 0 };
        pmc_ex.cb = sizeof(pmc_ex);
        if (GetProcessMemoryInfo(this->process_handle_, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc_ex), sizeof(pmc_ex))) {
            metrics_out.system_pagefile_usage_kb = pmc_ex.PagefileUsage / 1024;
            metrics_out.system_peak_pagefile_usage_kb = pmc_ex.PeakPagefileUsage / 1024;
            metrics_out.system_working_set_kb = pmc_ex.WorkingSetSize / 1024;
            metrics_out.system_peak_working_set_kb = pmc_ex.PeakWorkingSetSize / 1024;
            metrics_out.system_private_usage_kb = pmc_ex.PrivateUsage / 1024;
            metrics_out.system_quota_paged_pool_usage_kb = pmc_ex.QuotaPagedPoolUsage / 1024;
            metrics_out.system_quota_nonpaged_pool_usage_kb = pmc_ex.QuotaNonPagedPoolUsage / 1024;
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::collect_system_memory_info] System process memory stats collected.");
        }
        else {
            DWORD error = GetLastError();
            Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::collect_system_memory_info] Error getting process memory info: " + std::to_string(error));
        }
    }
    else {
        Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::collect_system_memory_info] Warning: Process handle or Impl not initialized for system memory stats.");
    }
#else
    Logger::getInstance().logAndPrint("[ResourceUtilization::Impl::collect_system_memory_info] System process memory stats collection not implemented for non-Windows.");
#endif
}
// --- 新增结束 ---