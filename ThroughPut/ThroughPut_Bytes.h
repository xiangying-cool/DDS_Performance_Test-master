// Throughput_Bytes.h
#pragma once

#include "DDSManager_Bytes.h"  // 只依赖 Bytes 版本
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

struct TestRoundResult;

namespace DDS {
    class DataWriter;
}

class Throughput_Bytes {
public:
    using ResultCallback = std::function<void(const TestRoundResult&)>;

    explicit Throughput_Bytes(DDSManager_Bytes& ddsManager, ResultCallback callback = nullptr);
    ~Throughput_Bytes();

    int runPublisher(const ConfigData& config);
    int runSubscriber(const ConfigData& config);

    bool waitForSubscriberReconnect(const std::chrono::seconds& timeout);

    void onDataReceived(const DDS::Bytes& sample, const DDS::SampleInfo& info);
    void onEndOfRound();

private:
    class WriterListener;

    DDSManager_Bytes& ddsManager_;
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
    std::chrono::steady_clock::time_point end_packet_time_;

    mutable std::mutex time_mutex_;  // 多线程安全
};