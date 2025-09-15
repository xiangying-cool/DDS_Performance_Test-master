// DDSZeroCopyManager.cpp
#include "DDSManager_ZeroCopyBytes.h"
#include "Logger.h"
#include "GloMemPool.h"

#include "ZRDDSDataReader.h"
#include "ZRDDSTypeSupport.h"
#include "ZRBuiltinTypesTypeSupport.h"
#include "ZRDDSDataWriter.h"

#include <iostream>
#include <sstream>
#include <random>
#include <chrono>
#include <cstring> // for memset

// Packet Header 结构（保持与 Bytes 版一致）
struct PacketHeader {
    uint32_t sequence;     // 序列号
    uint64_t timestamp;    // 发送时间（纳秒）
    uint8_t  packet_type;  // 0=普通数据, 1=结束包
};

// 内部 Listener 类 - 使用 ZeroCopyBytes 类型
class DDSManager_ZeroCopyBytes::MyDataReaderListener
    : public virtual DDS::SimpleDataReaderListener<
    DDS_ZeroCopyBytes,
    DDS_ZeroCopyBytesSeq,
    DDS::ZRDDSDataReader<DDS_ZeroCopyBytes, DDS_ZeroCopyBytesSeq>
    >
{
public:
    MyDataReaderListener(
        OnDataReceivedCallback_ZC dataCb,
        OnEndOfRoundCallback endCb
    ) : onDataReceived_(std::move(dataCb)), onEndOfRound_(std::move(endCb)) {
    }

    virtual void on_process_sample(
        DDS::DataReader*,
        const DDS_ZeroCopyBytes& sample,
        const DDS::SampleInfo& info
    ) override {
        if (!info.valid_data || sample.userLength < sizeof(PacketHeader)) {
            Logger::getInstance().logAndPrint("[DDSManager_ZeroCopyBytes] Invalid or short packet.");
            return;
        }

        const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(sample.userBuffer);

        if (hdr->packet_type == 1) {
            Logger::getInstance().logAndPrint(
                "[DDSManager_ZeroCopyBytes] Received end-of-round packet | seq=" +
                std::to_string(hdr->sequence) + " | ts=" + std::to_string(hdr->timestamp)
            );
            if (onEndOfRound_) {
                onEndOfRound_();
            }
            return;
        }

        // 正常数据包
        if (onDataReceived_) {
            onDataReceived_(sample, info);
        }
    }

private:
    OnDataReceivedCallback_ZC onDataReceived_;
    OnEndOfRoundCallback onEndOfRound_;
};

// 构造函数
DDSManager_ZeroCopyBytes::DDSManager_ZeroCopyBytes(const ConfigData& config, const std::string& xml_qos_file_path)
    : domain_id_(config.m_domainId)
    , topic_name_(config.m_topicName)
    , type_name_(config.m_typeName)
    , role_(config.m_isPositive ? "publisher" : "subscriber")
    , participant_factory_qos_name_(config.m_dpfQosName)
    , participant_qos_name_(config.m_dpQosName)
    , data_writer_qos_name_(config.m_writerQosName)
    , data_reader_qos_name_(config.m_readerQosName)
    , xml_qos_file_path_(xml_qos_file_path)
    , max_possible_size_(0)
    , global_buffer_(nullptr)
{
}

DDSManager_ZeroCopyBytes::~DDSManager_ZeroCopyBytes() {
    if (is_initialized_) {
        shutdown();
    }
}

bool DDSManager_ZeroCopyBytes::initialize(
    OnDataReceivedCallback_ZC dataCallback,
    OnEndOfRoundCallback endCallback
) {
    std::cout << "[DDSManager_ZeroCopyBytes] Initializing DDS entities...\n";

    const char* qosFilePath = xml_qos_file_path_.c_str();
    const char* p_lib_name = "default_lib";
    const char* p_prof_name = "default_profile";
    const char* pf_qos_name = participant_factory_qos_name_.empty() ? nullptr : participant_factory_qos_name_.c_str();
    const char* p_qos_name = participant_qos_name_.empty() ? nullptr : participant_qos_name_.c_str();

    // 获取工厂
    factory_ = DDS::DomainParticipantFactory::get_instance_w_profile(
        qosFilePath, p_lib_name, p_prof_name, pf_qos_name);
    if (!factory_) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Failed to get DomainParticipantFactory.\n";
        return false;
    }

    // 创建 Participant
    participant_ = factory_->create_participant_with_qos_profile(
        domain_id_, p_lib_name, p_prof_name, p_qos_name, nullptr, DDS::STATUS_MASK_NONE);
    if (!participant_) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Failed to create DomainParticipant.\n";
        return false;
    }

    // 注册类型 - 使用 ZeroCopyBytes TypeSupport
    DDS::ZeroCopyBytesTypeSupport* type_support = DDS::ZeroCopyBytesTypeSupport::get_instance();
    if (!type_support) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Failed to get ZeroCopyBytesTypeSupport instance.\n";
        return false;
    }

    const char* registered_type_name = type_support->get_type_name();
    if (!registered_type_name || strlen(registered_type_name) == 0) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Type name is null or empty.\n";
        return false;
    }

    if (type_support->register_type(participant_, registered_type_name) != DDS::RETCODE_OK) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Failed to register type '" << registered_type_name << "'.\n";
        return false;
    }

    // 创建 Topic
    topic_ = participant_->create_topic(
        topic_name_.c_str(), registered_type_name,
        DDS::TOPIC_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    if (!topic_) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Failed to create Topic '" << topic_name_ << "'.\n";
        return false;
    }

    // ========== 零拷贝关键步骤：预分配全局缓冲区 ==========
    size_t totalBufferSize = max_possible_size_ + DEFAULT_HEADER_RESERVE;
    global_buffer_ = static_cast<char*>(GloMemPool::allocate(totalBufferSize, __FILE__, __LINE__));
    if (!global_buffer_) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Failed to allocate global buffer for zero-copy.\n";
        return false;
    }
    std::cout << "[DDSManager_ZeroCopyBytes] Allocated global zero-copy buffer of size: " << totalBufferSize << " bytes\n";

    // 创建 Writer 或 Reader
    if (role_ == "publisher") {
        data_writer_ = participant_->create_datawriter_with_topic_and_qos_profile(
            topic_->get_name(), type_support,
            "default_lib", "default_profile", data_writer_qos_name_.c_str(),
            nullptr, DDS::STATUS_MASK_NONE);
        if (!data_writer_) {
            GloMemPool::deallocate(global_buffer_);
            global_buffer_ = nullptr;
            std::cerr << "[DDSManager_ZeroCopyBytes] Failed to create DataWriter.\n";
            return false;
        }
        std::cout << "[DDSManager_ZeroCopyBytes] Created DataWriter.\n";
    }
    else if (role_ == "subscriber") {
        void* mem = GloMemPool::allocate(sizeof(MyDataReaderListener), __FILE__, __LINE__);
        if (!mem) {
            GloMemPool::deallocate(global_buffer_);
            global_buffer_ = nullptr;
            std::cerr << "[DDSManager_ZeroCopyBytes] Memory allocation failed for listener.\n";
            return false;
        }
        listener_ = new (mem) MyDataReaderListener(std::move(dataCallback), std::move(endCallback));

        data_reader_ = participant_->create_datareader_with_topic_and_qos_profile(
            topic_->get_name(), type_support,
            "default_lib", "default_profile", data_reader_qos_name_.c_str(),
            listener_, DDS::DATA_AVAILABLE_STATUS);
        if (!data_reader_) {
            listener_->~MyDataReaderListener();
            GloMemPool::deallocate(listener_);
            listener_ = nullptr;
            GloMemPool::deallocate(global_buffer_);
            global_buffer_ = nullptr;
            std::cerr << "[DDSManager_ZeroCopyBytes] Failed to create DataReader.\n";
            return false;
        }
        std::cout << "[DDSManager_ZeroCopyBytes] Created DataReader with listener.\n";
    }
    else {
        std::cerr << "[DDSManager_ZeroCopyBytes] Invalid role: " << role_ << "\n";
        GloMemPool::deallocate(global_buffer_);
        global_buffer_ = nullptr;
        return false;
    }

    is_initialized_ = true;
    std::cout << "[DDSManager_ZeroCopyBytes] Initialization successful.\n";
    return true;
}

void DDSManager_ZeroCopyBytes::shutdown() {
    if (!factory_) return;

    if (listener_) {
        listener_->~MyDataReaderListener();
        GloMemPool::deallocate(listener_);
        listener_ = nullptr;
    }

    if (global_buffer_) {
        GloMemPool::deallocate(global_buffer_);
        global_buffer_ = nullptr;
    }

    if (participant_) {
        participant_->delete_contained_entities();
        factory_->delete_participant(participant_);
        participant_ = nullptr;
        topic_ = nullptr;
        data_writer_ = nullptr;
        data_reader_ = nullptr;
    }

    is_initialized_ = false;
    std::cout << "[DDSManager_ZeroCopyBytes] Shutdown completed.\n";
}

// DDSManager_ZeroCopyBytes.cpp
bool DDSManager_ZeroCopyBytes::ensureBufferSize(size_t user_data_size) {
    const size_t required_total = user_data_size + DEFAULT_HEADER_RESERVE;

    // 如果已有足够大的 buffer，无需重新分配
    if (global_buffer_ && max_possible_size_ >= user_data_size) {
        return true;
    }

    // 释放旧 buffer
    if (global_buffer_) {
        GloMemPool::deallocate(global_buffer_);
        global_buffer_ = nullptr;
    }

    // 分配新 buffer
    global_buffer_ = static_cast<char*>(GloMemPool::allocate(required_total, __FILE__, __LINE__));
    if (!global_buffer_) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Failed to allocate buffer for size: " << required_total << "\n";
        max_possible_size_ = 0;
        return false;
    }

    max_possible_size_ = user_data_size;  // 更新记录的最大用户数据长度

    Logger::getInstance().logAndPrint(
        "ZeroCopy buffer allocated: total=" + std::to_string(required_total) +
        ", userSize=" + std::to_string(user_data_size)
    );

    return true;
}

// 准备测试数据 (ZeroCopy 版本)
bool DDSManager_ZeroCopyBytes::prepareZeroCopyData(DDS_ZeroCopyBytes& sample, int dataSize, uint32_t sequence) {
    if (!global_buffer_) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Global buffer not allocated. Call initialize first!\n";
        return false;
    }

    const size_t headerSize = sizeof(PacketHeader);
    if (static_cast<size_t>(dataSize) < headerSize) {
        dataSize = headerSize; // 至少能放下 header
    }

    if (static_cast<size_t>(dataSize) > max_possible_size_) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Data size (" << dataSize
            << ") exceeds maximum possible size (" << max_possible_size_ << ").\n";
        return false;
    }

    // 设置结构体字段
    sample.totalLength = max_possible_size_ + DEFAULT_HEADER_RESERVE;
    sample.reservedLength = DEFAULT_HEADER_RESERVE;
    sample.value = global_buffer_;
    sample.userBuffer = global_buffer_ + DEFAULT_HEADER_RESERVE;
    sample.userLength = dataSize;

    // 填充 PacketHeader
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(sample.userBuffer);
    hdr->sequence = sequence;
    hdr->timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
    hdr->packet_type = 0; // 普通数据包

    // 填充 payload（可选）
    for (size_t i = headerSize; i < static_cast<size_t>(dataSize); ++i) {
        sample.userBuffer[i] = static_cast<DDS::Octet>((i + sequence) % 255);
    }

    Logger::getInstance().logAndPrint(
        "prepareZeroCopyData: seq=" + std::to_string(sequence) +
        " userLength=" + std::to_string(sample.userLength) +
        " reservedLength=" + std::to_string(DEFAULT_HEADER_RESERVE) +
        " totalLength=" + std::to_string(sample.totalLength)
    );

    return true;
}

// 准备结束包（统一格式）
bool DDSManager_ZeroCopyBytes::prepareEndZeroCopyData(DDS_ZeroCopyBytes& sample) {
    if (!global_buffer_) {
        std::cerr << "[DDSManager_ZeroCopyBytes] Global buffer not allocated.\n";
        return false;
    }

    const size_t headerSize = sizeof(PacketHeader);
    const size_t dataSize = headerSize;

    if (dataSize > max_possible_size_) {
        std::cerr << "[DDSManager_ZeroCopyBytes] End packet size exceeds limit.\n";
        return false;
    }

    sample.totalLength = max_possible_size_ + DEFAULT_HEADER_RESERVE;
    sample.reservedLength = DEFAULT_HEADER_RESERVE;
    sample.value = global_buffer_;
    sample.userBuffer = global_buffer_ + DEFAULT_HEADER_RESERVE;
    sample.userLength = dataSize;

    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(sample.userBuffer);
    hdr->sequence = 0xFFFFFFFF;
    hdr->timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
    hdr->packet_type = 1; // 结束包标记

    memset(sample.userBuffer + headerSize, 0, dataSize - headerSize);

    Logger::getInstance().logAndPrint(
        "prepareEndZeroCopyData: length=" + std::to_string(sample.userLength)
    );

    return true;
}