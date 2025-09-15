// Throughput_ZeroCopyBytes.cpp
#include "Throughput_ZeroCopyBytes.h" // <--- 确保包含头文件

#include "ZRDDSDataWriter.h"
#include "ZRDDSDataReader.h"
#include "ZRBuiltinTypes.h"
#include "ZRBuiltinTypesTypeSupport.h"

#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <mutex>

using namespace DDS;

// Packet Header 结构（保持与 Bytes 版本一致）
struct PacketHeader {
    uint32_t sequence;     // 序列号
    // uint64_t timestamp; // 当前未使用，可选
    uint8_t  packet_type;  // 0=数据包, 1=结束包
};

// ========================
// 内部类：WriterListener (专用于 ZeroCopy)
// ========================

class Throughput_ZeroCopyBytes::WriterListener : public virtual DDS::DataWriterListener {
public:
    WriterListener(std::atomic<bool>& flag, std::mutex& mtx, std::condition_variable& cv)
        : reconnected_flag_(flag), mutex_(mtx), cond_var_(cv), last_current_count_(0) {
    }

    void on_liveliness_lost(DataWriter*, const LivelinessLostStatus&) override {}
    void on_offered_deadline_missed(DataWriter*, const OfferedDeadlineMissedStatus&) override {}
    void on_offered_incompatible_qos(DataWriter*, const OfferedIncompatibleQosStatus&) override {}
    void on_publication_matched(DataWriter*, const PublicationMatchedStatus&) override {}

private:
    std::atomic<bool>& reconnected_flag_;
    std::mutex& mutex_;
    std::condition_variable& cond_var_;
    std::atomic<int32_t> last_current_count_;
};

// ========================
// 构造函数 & 析构
// ========================

Throughput_ZeroCopyBytes::Throughput_ZeroCopyBytes(DDSManager_ZeroCopyBytes& ddsManager, ResultCallback callback)
    : ddsManager_(ddsManager)
    , result_callback_(std::move(callback))
    , subscriber_reconnected_(false)
    , receivedCount_(0)
    , roundFinished_(false)
{
    writer_listener_ = std::make_unique<WriterListener>(
        subscriber_reconnected_,
        reconnect_mtx_,
        reconnect_cv_
    );

    DataWriter* writer = ddsManager_.get_data_writer();
    if (writer) {
        ReturnCode_t ret = writer->set_listener(writer_listener_.get(), DDS::SUBSCRIPTION_MATCHED_STATUS);
        if (ret != DDS::RETCODE_OK) {
            Logger::getInstance().logAndPrint("警告：无法为 DataWriter 设置监听器");
        }
    }
}

Throughput_ZeroCopyBytes::~Throughput_ZeroCopyBytes() = default;

// ========================
// 同步等待函数
// ========================

bool Throughput_ZeroCopyBytes::waitForSubscriberReconnect(const std::chrono::seconds& timeout) {
    std::unique_lock<std::mutex> lock(reconnect_mtx_);
    subscriber_reconnected_ = false;
    return reconnect_cv_.wait_for(lock, timeout, [this] { return subscriber_reconnected_.load(); });
}

void Throughput_ZeroCopyBytes::waitForRoundEnd() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this] { return roundFinished_.load(); });
}

bool Throughput_ZeroCopyBytes::waitForWriterMatch() {
    auto writer = ddsManager_.get_data_writer();
    if (!writer) return false;

    while (true) {
        PublicationMatchedStatus status{};
        ReturnCode_t ret = writer->get_publication_matched_status(status);
        if (ret == RETCODE_OK) {
            Logger::getInstance().logAndPrint(
                "Writer wait match(" + std::to_string(status.current_count) + "/1)"
            );
            if (status.current_count > 0) return true;
        }
        else {
            Logger::getInstance().logAndPrint("Error: Failed to get publication matched status.");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool Throughput_ZeroCopyBytes::waitForReaderMatch() {
    auto reader = ddsManager_.get_data_reader();
    if (!reader) return false;

    while (true) {
        SubscriptionMatchedStatus status{};
        ReturnCode_t ret = reader->get_subscription_matched_status(status);
        if (ret == RETCODE_OK) {
            Logger::getInstance().logAndPrint(
                "Reader wait match(" + std::to_string(status.current_count) + "/1)"
            );
            if (status.current_count > 0) return true;
        }
        else {
            Logger::getInstance().logAndPrint("Error: Failed to get subscription matched status.");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
// ========================
// runPublisher - 发送逻辑（零拷贝专用）
// ========================

int Throughput_ZeroCopyBytes::runPublisher(const ConfigData& config) {
    using WriterType = DDS::ZRDDSDataWriter<DDS::ZeroCopyBytes>;
    WriterType* writer = dynamic_cast<WriterType*>(ddsManager_.get_data_writer());
    if (!writer) {
        Logger::getInstance().logAndPrint("Throughput_ZeroCopyBytes: DataWriter 为空，无法发送");
        return -1;
    }

    const int round_index = config.m_activeLoop;
    const int minSize = config.m_minSize[round_index];
    const int maxSize = config.m_maxSize[round_index];
    const int sendCount = config.m_sendCount[round_index];
    const int sendPrintGap = config.m_sendPrintGap[round_index];

    // === 确保 Zero-Copy 缓冲区大小匹配当前轮次数据尺寸 ===
    if (!ddsManager_.ensureBufferSize(static_cast<size_t>(minSize))) {
        Logger::getInstance().error(
            "Throughput_ZeroCopyBytes: 无法为大小 " + std::to_string(minSize) +
            " 字节分配 Zero-Copy 缓冲区"
        );
        return -1;
    }

    if (!waitForWriterMatch()) {
        Logger::getInstance().logAndPrint("Throughput_ZeroCopyBytes: 等待 Subscriber 匹配超时");
        return -1;
    }

    std::ostringstream oss;
    oss << "第 " << (round_index + 1) << " 轮吞吐测试 | 发送: " << sendCount
        << " 条 | 数据大小: [" << minSize << ", " << maxSize << "]";
    Logger::getInstance().logAndPrint(oss.str());

    auto& resUtil = ResourceUtilization::instance();
    resUtil.initialize();
    SysMetrics start_metrics = resUtil.collectCurrentMetrics();

    DDS::ZeroCopyBytes sample;

    // 准备主数据样本（会使用新分配的 global_buffer）
    if (!ddsManager_.prepareZeroCopyData(sample, minSize, 0)) {
        Logger::getInstance().logAndPrint("Throughput_ZeroCopyBytes: 准备 ZeroCopy 测试数据失败");
        return -1;
    }

    char* userBuffer = sample.userBuffer;
    if (!userBuffer) {
        Logger::getInstance().error("Throughput_ZeroCopyBytes: userBuffer 为空");
        return -1;
    }

    // === 发送主循环 ===
    for (int j = 0; j < sendCount; ++j) {
        // 更新序列号
        *reinterpret_cast<uint32_t*>(userBuffer) = static_cast<uint32_t>(j);

        DDS::ReturnCode_t ret = writer->write(sample, DDS_HANDLE_NIL_NATIVE);
        if (ret == DDS::RETCODE_OK) {
            static int cnt = 0;
            if (++cnt % sendPrintGap == 0) {
                Logger::getInstance().logAndPrint("已发送 " + std::to_string(cnt) + " 条");
            }
        }
        else {
            Logger::getInstance().error("Write failed: " + std::to_string(ret));
        }
    }

    // 等待所有数据被确认
    DDS::Duration_t timeout = { 10, 0 };
    writer->wait_for_acknowledgments(timeout);

    // === 发送结束包（标记本轮结束）===
    ddsManager_.prepareEndZeroCopyData(sample);
    for (int k = 0; k < 3; ++k) {
        writer->write(sample, DDS_HANDLE_NIL_NATIVE);
        Logger::getInstance().logAndPrint("结束包发送第 " + std::to_string(k + 1) + " 次");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 收集资源使用情况
    SysMetrics end_metrics = resUtil.collectCurrentMetrics();
    if (result_callback_) {
        result_callback_(TestRoundResult{ round_index + 1, start_metrics, end_metrics }); // <--- 此处应能正常工作
    }

    Logger::getInstance().logAndPrint("第 " + std::to_string(round_index + 1) + " 轮发送完成 (ZeroCopy)");
    return 0;
}

// ========================
// runSubscriber - 接收逻辑（零拷贝专用）
// ========================

int Throughput_ZeroCopyBytes::runSubscriber(const ConfigData& config) {
    DDS::DataReader* raw_reader = ddsManager_.get_data_reader();
    if (!raw_reader) {
        Logger::getInstance().logAndPrint("Throughput_ZeroCopyBytes: DataReader 为空");
        return -1;
    }

    Logger::getInstance().logAndPrint("DataReader 已就绪，等待数据...");

    const int round_index = config.m_activeLoop;
    const int expected = config.m_sendCount[round_index];
    const int avg_packet_size = config.m_minSize[round_index];  // 假设 min == max

    // === 动态调整接收端缓冲区大小 ===
    if (!ddsManager_.ensureBufferSize(static_cast<size_t>(avg_packet_size))) {
        Logger::getInstance().error(
            "Throughput_ZeroCopyBytes: Subscriber 无法分配足够大的 Zero-Copy 缓冲区"
        );
        return -1;
    }

    if (!waitForReaderMatch()) {
        Logger::getInstance().logAndPrint("Throughput_ZeroCopyBytes: 等待 Publisher 匹配超时");
        return -1;
    }

    Logger::getInstance().logAndPrint("第 " + std::to_string(round_index + 1) + " 轮吞吐量测试开始 (ZeroCopy)");

    auto& resUtil = ResourceUtilization::instance();
    resUtil.initialize();
    SysMetrics start_metrics = resUtil.collectCurrentMetrics();

    // 重置状态
    receivedCount_.store(0);
    roundFinished_.store(false);

    {
        std::lock_guard<std::mutex> lock(time_mutex_);
        first_packet_time_ = std::chrono::steady_clock::time_point(); // zero 初始化
        end_packet_time_ = std::chrono::steady_clock::time_point();
    }

    // === 阻塞等待测试结束信号 ===
    waitForRoundEnd();  // 内部调用 cv_.wait(...) 直到 onEndOfRound() 触发

    // === 获取计时结果 ===
    std::chrono::steady_clock::time_point start_time, end_time;
    {
        std::lock_guard<std::mutex> lock(time_mutex_);
        start_time = first_packet_time_;
        end_time = end_packet_time_;
    }

    // 如果没收到任何包
    if (start_time.time_since_epoch().count() == 0) {
        Logger::getInstance().logAndPrint("警告：未收到任何有效数据包");
    }

    // === 计算性能指标 ===
    int received = receivedCount_.load();
    double duration_seconds = 0.0;
    double throughput_pps = 0.0;
    double throughput_mbps = 0.0;

    if (start_time.time_since_epoch().count() != 0 &&
        end_time.time_since_epoch().count() != 0 &&
        end_time >= start_time) {

        auto duration = end_time - start_time;
        duration_seconds = std::chrono::duration<double>(duration).count();
        throughput_pps = duration_seconds > 0 ? received / duration_seconds : 0.0;
        throughput_mbps = (static_cast<double>(avg_packet_size) *
            static_cast<double>(received) * 8.0 /
            (1024.0 * 1024.0)) / max(duration_seconds, 1e-9);
    }

    // === 丢包率 ===
    int lost = expected - received;
    double lossRate = expected > 0 ? static_cast<double>(lost) / expected * 100.0 : 0.0;

    // === 上报资源使用 ===
    SysMetrics end_metrics = resUtil.collectCurrentMetrics();
    if (result_callback_) {
        result_callback_(TestRoundResult{ round_index + 1, start_metrics, end_metrics }); // <--- 此处也应能正常工作
    }

    // === 输出结果 ===
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "吞吐量测试 (ZeroCopy) | 第 " << (round_index + 1) << " 轮 | "
        << "接收: " << received << " 包 | "
        << "丢包: " << lost << " 包 | "
        << "丢包率: " << lossRate << "% | "
        << "耗时: " << (duration_seconds * 1000.0) << " ms | "
        << "吞吐: " << throughput_pps << " pps | "
        << "带宽: " << throughput_mbps << " Mbps";

    Logger::getInstance().logAndPrint(oss.str());

    return 0;
}

// ========================
// 回调函数实现（供外部 initialize 时传入）
// ========================

void Throughput_ZeroCopyBytes::onDataReceived(const DDS_ZeroCopyBytes& /*sample*/, const DDS::SampleInfo& info) {
    if (!info.valid_data) return;

    int64_t count = receivedCount_.fetch_add(1, std::memory_order_relaxed) + 1;

    // 记录第一个包的时间
    if (count == 1) {
        std::lock_guard<std::mutex> lock(time_mutex_);
        first_packet_time_ = std::chrono::steady_clock::now();
        Logger::getInstance().logAndPrint("收到第一个数据包，开始计时...");
    }
}

void Throughput_ZeroCopyBytes::onEndOfRound() {
    // 记录结束时间
    {
        std::lock_guard<std::mutex> lock(time_mutex_);
        end_packet_time_ = std::chrono::steady_clock::now();
    }

    roundFinished_.store(true);
    cv_.notify_one();

    Logger::getInstance().logAndPrint("[Throughput_ZeroCopyBytes] 测试轮次结束信号已触发");
}