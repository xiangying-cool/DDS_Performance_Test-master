// Logger.cpp
#include "Logger.h"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

// Logger 的内部实现类
class Logger::Impl {
public:
    std::ofstream file_;                             // 日志文件流
    std::string log_directory_;                      // 日志目录
    std::string log_file_prefix_ = "log_";          // 日志文件名前缀
    std::string log_file_suffix_ = ".log";          // 日志文件名后缀
    bool isInitialized_ = false;                     // 是否已初始化
    mutable std::atomic<int> file_counter_{ 0 };    // 用于生成唯一文件名的原子计数器

    // 日志队列，用于异步写入
    std::queue<std::string> log_queue_;
    std::mutex queue_mutex_;                        // 保护日志队列的互斥锁
    std::condition_variable cv_;                    // 条件变量，用于通知写入线程
    std::atomic<bool> stop_{ false };               // 停止标志
    std::thread writer_thread_;                     // 后台写入线程

    // 结构体，用于处理和格式化时间戳
    struct LogTimestamp {
        int year, month, day, hour, minute, second, millisecond;

        // 格式化为 "YYYY-MM-DD HH:MM:SS"
        std::string formatTime() const {
            std::ostringstream oss;
            oss << year << '-'
                << std::setfill('0') << std::setw(2) << month << '-'
                << std::setfill('0') << std::setw(2) << day << ' '
                << std::setfill('0') << std::setw(2) << hour << ':'
                << std::setfill('0') << std::setw(2) << minute << ':'
                << std::setfill('0') << std::setw(2) << second;
            return oss.str();
        }

        // 格式化为 "YYYY-MM-DD HH:MM:SS.mmm"
        std::string formatTimeWithMs() const {
            std::ostringstream oss;
            oss << year << '-'
                << std::setfill('0') << std::setw(2) << month << '-'
                << std::setfill('0') << std::setw(2) << day << ' '
                << std::setfill('0') << std::setw(2) << hour << ':'
                << std::setfill('0') << std::setw(2) << minute << ':'
                << std::setfill('0') << std::setw(2) << second
                << '.' << std::setfill('0') << std::setw(3) << millisecond;
            return oss.str();
        }

        // 格式化为 "YYYYMMDD_HHMMSS_mmm" (用于文件名)
        std::string formatFilename() const {
            std::ostringstream oss;
            oss << year
                << std::setfill('0') << std::setw(2) << month
                << std::setfill('0') << std::setw(2) << day
                << "_"
                << std::setfill('0') << std::setw(2) << hour
                << std::setfill('0') << std::setw(2) << minute
                << std::setfill('0') << std::setw(2) << second
                << "_" << std::setfill('0') << std::setw(3) << millisecond;
            return oss.str();
        }
    };

    // 获取当前时间戳
    LogTimestamp getLogTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto time_t = std::chrono::system_clock::to_time_t(now);

        tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time_t); // Windows 线程安全版本
#else
        localtime_r(&time_t, &tm); // POSIX 线程安全版本
#endif

        auto ms_count = ms.time_since_epoch().count();
        auto sec_count = std::chrono::duration_cast<std::chrono::seconds>(ms.time_since_epoch()).count();
        int millisecond = static_cast<int>(ms_count - sec_count * 1000);

        return LogTimestamp{
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec,
            millisecond
        };
    }

    // 获取带毫秒的时间字符串
    std::string getCurrentTimeStr() const {
        return getLogTimestamp().formatTimeWithMs();
    }

    // 生成唯一的日志文件名
    std::string generateLogFileName() const {
        auto ts = getLogTimestamp();
        int seq = file_counter_.fetch_add(1); // 原子递增
        std::ostringstream oss;
        oss << log_file_prefix_
            << ts.formatFilename()
            << "_" << std::setfill('0') << std::setw(2) << seq
            << log_file_suffix_;
        // 使用 std::filesystem::path 确保正确的路径分隔符
        return (std::filesystem::path(log_directory_) / oss.str()).string();
    }

    // 后台写入线程函数
    void backgroundWrite() {
        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 等待队列非空或收到停止信号
            cv_.wait(lock, [this] { return !log_queue_.empty() || stop_.load(); });

            // 如果收到停止信号且队列为空，则退出循环
            if (stop_.load() && log_queue_.empty()) {
                break;
            }

            // 取出队列头部的日志行
            std::string logLine = std::move(log_queue_.front());
            log_queue_.pop();
            lock.unlock(); // 释放锁，以便其他线程可以继续入队

            // 写入文件
            if (isInitialized_ && file_.is_open()) {
                file_ << logLine << '\n';
                file_.flush(); // 确保立即写入磁盘
            }
        }

        // 线程退出前，写入结束标记并关闭文件
        if (isInitialized_ && file_.is_open()) {
            file_ << "[" << getCurrentTimeStr() << "] === ZRDDS-Perf-Bench Log Finished ===\n";
            file_.flush();
            file_.close();
            isInitialized_ = false;
        }
    }

    // 将日志行加入队列
    void pushLog(const std::string& logLine) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_.load()) return; // 如果已停止，则不入队
            log_queue_.push(logLine);
        }
        cv_.notify_one(); // 通知后台线程有新日志
    }

    // 初始化日志系统
    bool initialize(
        const std::string& logDirectory,
        const std::string& filePrefix,
        const std::string& fileSuffix)
    {
        if (isInitialized_) return true; // 已初始化则直接返回

        log_directory_ = logDirectory;
        log_file_prefix_ = filePrefix;
        log_file_suffix_ = fileSuffix;

        // 创建日志目录（如果不存在）
        if (!std::filesystem::exists(log_directory_)) {
            std::cout << "[Logger] 日志目录不存在，正在创建: " << log_directory_ << std::endl;
            if (!std::filesystem::create_directories(log_directory_)) {
                std::cerr << "[ERROR] 创建日志目录失败，请检查路径权限或磁盘状态: " << log_directory_ << std::endl;
                return false;
            }
            std::cout << "[Logger] 日志目录已创建: " << log_directory_ << std::endl;
        }

        // 生成并打开日志文件
        std::string logFileName = generateLogFileName();
        file_.open(logFileName, std::ios::out | std::ios::app); // 追加模式
        if (!file_.is_open()) {
            std::cerr << "[Error] 无法打开日志文件: " << logFileName << std::endl;
            return false;
        }

        // 写入日志开始标记
        std::string header = "[" + getCurrentTimeStr() + "] === ZRDDS-Perf-Bench Log Started ===\n"
            "Log File: " + logFileName + "\n"
            "----------------------------------------";
        pushLog(header);

        isInitialized_ = true;
        std::cout << "[Logger] 日志系统已启动\n";

        return true;
    }

    // 关闭日志系统
    void close() {
        if (stop_.exchange(true)) return; // 原子地设置停止标志，如果已停止则直接返回
        cv_.notify_all(); // 唤醒所有等待的线程（主要是后台写入线程）
    }

    // 构造函数：启动后台写入线程
    Impl() {
        writer_thread_ = std::thread(&Impl::backgroundWrite, this);
    }

    // 析构函数：确保资源被正确释放
    ~Impl() {
        close(); // 发送停止信号
        if (writer_thread_.joinable()) {
            writer_thread_.join(); // 等待后台线程结束
        }
    }
};


// Logger 类的实现

// 构造函数：创建 Impl 对象
Logger::Logger()
    : pImpl_(std::make_unique<Impl>())
{

}

// 析构函数：unique_ptr 会自动删除 Impl 对象
Logger::~Logger() = default;

// 公共接口：初始化
bool Logger::initialize(const std::string& logDirectory,
    const std::string& filePrefix,
    const std::string& fileSuffix)
{
    return pImpl_->initialize(logDirectory, filePrefix, fileSuffix);
}

// 公共接口：静态设置方法
void Logger::setupLogger(const std::string& logDirectory,
    const std::string& filePrefix,
    const std::string& fileSuffix)
{
    auto& logger = Logger::getInstance(); // 获取单例
    bool success = logger.initialize(logDirectory, filePrefix, fileSuffix);
    if (!success) {
        std::cerr << "日志初始化失败，请检查目录权限或路径\n";
    }
}

// 公共接口：记录普通日志
void Logger::log(const std::string& msg) {
    if (pImpl_->isInitialized_) {
        std::string line = "[LOG] " + pImpl_->getCurrentTimeStr() + " " + msg;
        pImpl_->pushLog(line);
    }
}

// 公共接口：记录 Info 日志
void Logger::info(const std::string& msg) {
    if (pImpl_->isInitialized_) {
        std::string line = "[INFO] " + pImpl_->getCurrentTimeStr() + " " + msg;
        pImpl_->pushLog(line);
    }
}

// 公共接口：记录 Error 日志
void Logger::error(const std::string& msg) {
    std::string line = "[ERROR] " + pImpl_->getCurrentTimeStr() + " " + msg;
    if (!pImpl_->isInitialized_) {
        // 如果日志未初始化，至少打印到标准错误
        std::cerr << line << " [Warning] (日志未启用)\n";
        return;
    }
    pImpl_->pushLog(line);
}

// 公共接口：记录配置信息
void Logger::logConfig(const std::string& configInfo) {
    std::ostringstream oss;
    oss << "\n=== 测试配置 ===\n" << configInfo;
    if (pImpl_->isInitialized_) {
        pImpl_->pushLog(oss.str());
    }
    else {
        std::cerr << "[WARNING] 尝试写入配置日志，但日志未初始化\n" << configInfo << std::endl;
    }
}

// 公共接口：记录结果信息
void Logger::logResult(const std::string& result) {
    std::ostringstream oss;
    oss << "\n=== 测试结果 ===\n" << result;
    if (pImpl_->isInitialized_) {
        pImpl_->pushLog(oss.str());
    }
    else {
        std::cerr << "[WARNING] 尝试写入结果日志，但日志未初始化\n" << result << std::endl;
    }
}

// 公共接口：记录并打印
void Logger::logAndPrint(const std::string& msg) {
    std::cout << msg << std::endl; // 先打印到控制台
    if (pImpl_->isInitialized_) {
        std::string line = "[LOG] " + pImpl_->getCurrentTimeStr() + " " + msg;
        pImpl_->pushLog(line); // 再加入日志队列
    }
}

// 公共接口：关闭日志
void Logger::close() {
    pImpl_->close(); // 调用 Impl 的 close 方法
}