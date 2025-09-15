// Throughput_ZeroCopyBytes.h
#pragma once

#include "ConfigData.h"
#include "TestRoundResult.h"
#include "SysMetrics.h"
#include "ResourceUtilization.h"
#include "Logger.h"

#include "DDSManager_ZeroCopyBytes.h"  // 包含 manager 定义

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace DDS {
    class DataWriter;
}

class Throughput_ZeroCopyBytes {
public:
    using ResultCallback = std::function<void(const TestRoundResult&)>;

    explicit Throughput_ZeroCopyBytes(DDSManager_ZeroCopyBytes& ddsManager, ResultCallback callback = nullptr);
    ~Throughput_ZeroCopyBytes();

    int runPublisher(const ConfigData& config);
    int runSubscriber(const ConfigData& config);

    bool waitForSubscriberReconnect(const std::chrono::seconds& timeout);

    void onDataReceived(const DDS::ZeroCopyBytes& sample, const DDS::SampleInfo& info);
    void onEndOfRound();

private:
    class WriterListener;

    DDSManager_ZeroCopyBytes& ddsManager_;
    ResultCallback result_callback_;

    std::atomic<int> receivedCount_{ 0 };
    std::atomic<bool> roundFinished_{ false };
    std::mutex mtx_;
    std::condition_variable cv_;

    std::atomic<bool> subscriber_reconnected_{ false };
    std::mutex reconnect_mtx_;
    std::condition_variable reconnect_cv_;

    std::unique_ptr<WriterListener> writer_listener_;

    void waitForRoundEnd();
    bool waitForWriterMatch();
    bool waitForReaderMatch();

    std::chrono::steady_clock::time_point first_packet_time_;
    std::chrono::steady_clock::time_point end_packet_time_; // 结束包收到时间

    mutable std::mutex time_mutex_; // 保护 time_point 的修改（避免多线程竞争）
};