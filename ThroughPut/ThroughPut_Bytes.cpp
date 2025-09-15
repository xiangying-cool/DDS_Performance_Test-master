// Throughput_Bytes.cpp
#include "Throughput_Bytes.h"

#include "Logger.h"
#include "ResourceUtilization.h"
#include "TestRoundResult.h"
#include "SysMetrics.h"

#include "ZRDDSDataWriter.h"
#include "ZRDDSDataReader.h"
#include "ZRBuiltinTypes.h"

#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

using namespace DDS;
struct PacketHeader {
    uint32_t sequence;     // 序列号
    //uint64_t timestamp;    // 发送时间（纳秒）
    uint8_t  packet_type;
};

// ========================
// 内部类：WriterListener
// ========================

class Throughput_Bytes::WriterListener : public virtual DDS::DataWriterListener {
public:
    WriterListener(std::atomic<bool>& flag, std::mutex& mtx, std::condition_variable& cv)
        : reconnected_flag_(flag), mutex_(mtx), cond_var_(cv), last_current_count_(0) {
    }

    void on_liveliness_lost(DataWriter*, const LivelinessLostStatus&) override {}
    void on_offered_deadline_missed(DataWriter*, const OfferedDeadlineMissedStatus&) override {}
    void on_offered_incompatible_qos(DataWriter*, const OfferedIncompatibleQosStatus&) override {}
    void on_publication_matched(DataWriter*, const PublicationMatchedStatus&) override {}

    /*void on_subscription_matched(DataWriter*, const SubscriptionMatchedStatus& status) override {
        int32_t current = status.current_count;
        int32_t previous = last_current_count_.load();

        last_current_count_.store(current);

        if (current > 0 && previous == 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            reconnected_flag_.store(true);
            cond_var_.notify_all();
            Logger::getInstance().logAndPrint("Throughput_Bytes: 检测到订阅者重新上线");
        }
    }*/

private:
    std::atomic<bool>& reconnected_flag_;
    std::mutex& mutex_;
    std::condition_variable& cond_var_;
    std::atomic<int32_t> last_current_count_;
};

// ========================
// 构造函数 & 析构
// ========================

Throughput_Bytes::Throughput_Bytes(DDSManager_Bytes& ddsManager, ResultCallback callback)
    : ddsManager_(ddsManager)
    , result_callback_(std::move(callback))
    , subscriber_reconnected_(false)
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

Throughput_Bytes::~Throughput_Bytes() = default;

// ========================
// 同步函数
// ========================

bool Throughput_Bytes::waitForSubscriberReconnect(const std::chrono::seconds& timeout) {
    std::unique_lock<std::mutex> lock(reconnect_mtx_);
    subscriber_reconnected_ = false;
    return reconnect_cv_.wait_for(lock, timeout, [this] { return subscriber_reconnected_.load(); });
}

void Throughput_Bytes::waitForRoundEnd() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this] { return roundFinished_.load(); });
}

bool Throughput_Bytes::waitForWriterMatch() {
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

bool Throughput_Bytes::waitForReaderMatch() {
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
// runPublisher - 发送逻辑
// ========================

int Throughput_Bytes::runPublisher(const ConfigData& config) {
    using WriterType = DDS::ZRDDSDataWriter<DDS::Bytes>;
    WriterType* writer = dynamic_cast<WriterType*>(ddsManager_.get_data_writer());
    if (!writer) {
        Logger::getInstance().logAndPrint("Throughput_Bytes: DataWriter 为空，无法发送");
        return -1;
    }

    const int round_index = config.m_activeLoop;
    const int minSize = config.m_minSize[round_index];
    const int maxSize = config.m_maxSize[round_index];
    const int sendCount = config.m_sendCount[round_index];
    const int sendPrintGap = config.m_sendPrintGap[round_index];

    if (!waitForWriterMatch()) {
        Logger::getInstance().logAndPrint("Throughput_Bytes: 等待 Subscriber 匹配超时");
        return -1;
    }

    std::ostringstream oss;
    oss << "第 " << (round_index + 1) << " 轮吞吐测试 | 发送: " << sendCount
        << " 条 | 数据大小: [" << minSize << ", " << maxSize << "]";
    Logger::getInstance().logAndPrint(oss.str());

    auto& resUtil = ResourceUtilization::instance();
    resUtil.initialize();
    SysMetrics start_metrics = resUtil.collectCurrentMetrics();

    DDS::Bytes sample;

    // 准备测试数据（只准备一次，后续复用 buffer）
    if (!ddsManager_.prepareBytesData(sample, minSize, maxSize, 0, 0)) {
        Logger::getInstance().logAndPrint("Throughput_Bytes: 准备测试数据失败");
        return -1;
    }

    // 获取 contiguous buffer 指针（提前获取，避免重复调用）
    uint8_t* buffer = sample.value.get_contiguous_buffer();
    if (!buffer) {
        Logger::getInstance().error("Throughput_Bytes: 内存 buffer 为空");
        return -1;
    }

    // === 发送主循环 ===
    for (int j = 0; j < sendCount; ++j) {
        *reinterpret_cast<uint32_t*>(buffer) = j;

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
    writer->wait_for_acknowledgments({ 10, 0 });  // 10秒超时

    // === 发送结束包（标记本轮结束）===
    // === 发送结束包 ===
    // 发送结束包
    ddsManager_.cleanupBytesData(sample);
    if (ddsManager_.prepareEndBytesData(sample, minSize)) {
        if (sample.value.length() > 0) {
            Logger::getInstance().logAndPrint("发送结束包，长度=" + std::to_string(sample.value.length()));
        }
        else {
            Logger::getInstance().logAndPrint("错误：结束包长度为 0");
            return -1;
        }
        for (int k = 0; k < 3; ++k) {
            writer->write(sample, DDS_HANDLE_NIL_NATIVE);
            Logger::getInstance().logAndPrint("结束包发送第 " + std::to_string(k + 1) + " 次");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    ddsManager_.cleanupBytesData(sample);

    // 收集资源使用情况
    SysMetrics end_metrics = resUtil.collectCurrentMetrics();
    if (result_callback_) {
        result_callback_(TestRoundResult{ round_index + 1, start_metrics, end_metrics });
    }

    Logger::getInstance().logAndPrint("第 " + std::to_string(round_index + 1) + " 轮发送完成");
    return 0;
}

// ========================
// runSubscriber - 接收逻辑
// ========================
int Throughput_Bytes::runSubscriber(const ConfigData& config) {
    // 不需要 take，只需要确保 DataReader 存在
    DDS::DataReader* raw_reader = ddsManager_.get_data_reader();
    if (!raw_reader) {
        Logger::getInstance().logAndPrint("Throughput_Bytes: DataReader 为空");
        return -1;
    }

    Logger::getInstance().logAndPrint("DataReader 已就绪，等待数据...");

    const int round_index = config.m_activeLoop;

    if (!waitForReaderMatch()) {
        Logger::getInstance().logAndPrint("Throughput_Bytes: 等待 Publisher 匹配超时");
        return -1;
    }

    Logger::getInstance().logAndPrint("第 " + std::to_string(round_index + 1) + " 轮吞吐量测试开始");

    auto& resUtil = ResourceUtilization::instance();
    resUtil.initialize();
    SysMetrics start_metrics = resUtil.collectCurrentMetrics();

    // 重置状态
    receivedCount_.store(0);
    roundFinished_.store(false);

    // 用于计时（由回调设置）
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    bool first_packet_received = false;

    // === 设置本地 lambda 捕获计时变量 ===
    // 注意：这里不能直接改 onDataReceived，而是让回调更新这些状态
    // 所以你需要一个方式让外部知道第一个包时间
    // 我们用成员变量或共享状态（推荐用成员变量）

    // 👇 假设你已将以下变量提升为 Throughput_Bytes 的成员：
    //     std::chrono::steady_clock::time_point first_packet_time_;
    //     std::chrono::steady_clock::time_point end_packet_time_;

    // 这里我们假设你已经通过其他方式记录了这两个时间（见下方说明）

    // === 阻塞等待测试结束信号 ===
    waitForRoundEnd();  // 会阻塞直到 onEndOfRound() 被调用

    // === 测试结束，读取计时结果 ===
    start_time = first_packet_time_;   // 来自 onDataReceived 的记录
    end_time = end_packet_time_;       // 来自 onEndOfRound 的记录

    // 如果没收到任何包
    if (start_time.time_since_epoch().count() == 0) {
        Logger::getInstance().logAndPrint("警告：未收到任何有效数据包");
    }

    // === 计算吞吐量 ===
    int received = receivedCount_.load();
    double duration_seconds = 0.0;
    double throughput_pps = 0.0;
    double throughput_mbps = 0.0;

    int avg_packet_size = config.m_minSize[round_index];

    if (start_time.time_since_epoch().count() != 0 &&
        end_time.time_since_epoch().count() != 0 &&
        end_time >= start_time) {

        auto duration = end_time - start_time;
        duration_seconds = std::chrono::duration<double>(duration).count();

        throughput_pps = duration_seconds > 0 ? static_cast<double>(received) / duration_seconds : 0.0;

        if (duration_seconds > 1e-9) {
            throughput_mbps = (static_cast<double>(avg_packet_size) *
                static_cast<double>(received) * 8.0 /
                (1024.0 * 1024.0)) / duration_seconds;
        }
    }

    // === 计算丢包数和丢包率 ===
    int expected = config.m_sendCount[round_index];
    int lost = expected - received;
    double lossRate = expected > 0 ? (double)lost / expected * 100.0 : 0.0;

    // === 上报资源使用 ===
    SysMetrics end_metrics = resUtil.collectCurrentMetrics();
    if (result_callback_) {
        result_callback_(TestRoundResult{ round_index + 1, start_metrics, end_metrics });
    }

    // === 输出结果 ===
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "吞吐量测试 (Listener模式) | 第 " << (round_index + 1) << " 轮 | "
        << "接收: " << received << " 包 | "
        << "丢包: " << lost << " 包 | "
        << "丢包率: " << lossRate << "% | "
        << "耗时: " << duration_seconds * 1000.0 << " ms | "
        << "吞吐: " << throughput_pps << " pps | "
        << "带宽: " << throughput_mbps << " Mbps";

    Logger::getInstance().logAndPrint(oss.str());

    return 0;
}

// ========================
// 回调函数
// ========================

void Throughput_Bytes::onDataReceived(const DDS::Bytes& /*sample*/, const DDS::SampleInfo& info) {
    if (!info.valid_data) return;

    int64_t count = receivedCount_.fetch_add(1, std::memory_order_relaxed) + 1;

    // 记录第一个包的时间
    if (count == 1) {
        std::lock_guard<std::mutex> lock(time_mutex_);
        first_packet_time_ = std::chrono::steady_clock::now();
        Logger::getInstance().logAndPrint("收到第一个数据包，开始计时...");
    }
}

void Throughput_Bytes::onEndOfRound() {
    // 记录结束时间
    {
        std::lock_guard<std::mutex> lock(time_mutex_);
        end_packet_time_ = std::chrono::steady_clock::now();
    }

    roundFinished_.store(true);
    cv_.notify_one();

    Logger::getInstance().logAndPrint("[Throughput_Bytes] 测试轮次结束信号已触发");
}