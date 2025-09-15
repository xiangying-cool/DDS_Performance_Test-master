// Logger.h
#pragma once
#include <string>
#include <memory>
#include <thread>

// 日志记录器类 (单例模式)
class Logger {
public:
    // 获取单例实例的静态方法
    static Logger& getInstance() {
        static Logger instance; // 线程安全的局部静态变量 (C++11 起)
        return instance;
    }

    // 禁止拷贝构造和拷贝赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 初始化日志系统 (指定目录、文件前缀、后缀)
    bool initialize(
        const std::string& logDirectory,
        const std::string& filePrefix,
        const std::string& fileSuffix
    );

    // 静态辅助方法，用于方便地设置单例实例
    static void setupLogger(
        const std::string& logDirectory,
        const std::string& filePrefix,
        const std::string& fileSuffix
    );

    // 记录日志信息 (不打印到控制台)
    void log(const std::string& message);
    // 记录配置信息
    void logConfig(const std::string& configInfo);
    // 记录结果信息
    void logResult(const std::string& result);
    // 记录日志信息并同时打印到控制台
    void logAndPrint(const std::string& message);
    // 记录 Info 级别信息
    void info(const std::string& msg);
    // 记录 Error 级别信息
    void error(const std::string& msg);
    // 关闭日志系统
    void close();

private:
    // 私有构造函数和析构函数，防止外部实例化
    Logger();
    ~Logger();

    // Pimpl (Pointer to Implementation) 模式
    // 将具体实现细节隐藏在 Impl 类中
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};