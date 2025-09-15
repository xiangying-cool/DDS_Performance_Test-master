// DDSManager_Bytes.cpp
#include "DDSManager_Bytes.h"
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

// Packet Header 结构（定义在 .cpp 内部即可）
struct PacketHeader {
    uint32_t sequence;     // 序列号
    uint64_t timestamp;    // 发送时间（纳秒）
    uint8_t  packet_type;
};

// 内部 Listener 类 - 使用 Bytes 类型
class DDSManager_Bytes::MyDataReaderListener
    : public virtual DDS::SimpleDataReaderListener<DDS::Bytes, DDS::BytesSeq, DDS::ZRDDSDataReader<DDS::Bytes, DDS::BytesSeq>>
{
public:
    MyDataReaderListener(
        OnDataReceivedCallback_Bytes dataCb,
        OnEndOfRoundCallback endCb
    ) : onDataReceived_(std::move(dataCb)), onEndOfRound_(std::move(endCb)) {
    }

    void on_process_sample(
        DDS::DataReader*,
        const DDS::Bytes& sample,
        const DDS::SampleInfo& info
    ) override {
        if (!info.valid_data || sample.value.length() < sizeof(PacketHeader)) {
            Logger::getInstance().logAndPrint("[DDSManager_Bytes] 收到无效或过短的数据包");
            return;
        }

        const uint8_t* buffer = sample.value.get_contiguous_buffer();
        if (!buffer) {
            Logger::getInstance().error("[DDSManager_Bytes] buffer 为空");
            return;
        }

        const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(buffer);

        // 判断是否为结束包
        if (hdr->packet_type == 1) {
            Logger::getInstance().logAndPrint(
                "[DDSManager_Bytes] 收到结束包 | seq=" + std::to_string(hdr->sequence) +
                " | ts=" + std::to_string(hdr->timestamp) +
                " | length=" + std::to_string(sample.value.length())
            );
            if (onEndOfRound_) {
                onEndOfRound_();
            }
            return;
        }

        // 普通数据包
        if (onDataReceived_) {
            onDataReceived_(sample, info);
        }
    }

private:
    OnDataReceivedCallback_Bytes onDataReceived_;
    OnEndOfRoundCallback onEndOfRound_;
};

// 构造函数
DDSManager_Bytes::DDSManager_Bytes(const ConfigData& config, const std::string& xml_qos_file_path)
    : domain_id_(config.m_domainId)
    , topic_name_(config.m_topicName)
    , type_name_(config.m_typeName)
    , role_(config.m_isPositive ? "publisher" : "subscriber")
    , participant_factory_qos_name_(config.m_dpfQosName)
    , participant_qos_name_(config.m_dpQosName)
    , data_writer_qos_name_(config.m_writerQosName)
    , data_reader_qos_name_(config.m_readerQosName)
    , xml_qos_file_path_(xml_qos_file_path)
{
}

DDSManager_Bytes::~DDSManager_Bytes() {
    if (is_initialized_) {
        shutdown();
    }
}

bool DDSManager_Bytes::initialize(
    OnDataReceivedCallback_Bytes dataCallback,
    OnEndOfRoundCallback endCallback
) {
    Logger::getInstance().logAndPrint("[DDSManager_Bytes] 开始初始化 DDS 实体...");

    const char* qosFilePath = xml_qos_file_path_.c_str();
    const char* p_lib_name = "default_lib";
    const char* p_prof_name = "default_profile";
    const char* pf_qos_name = participant_factory_qos_name_.empty() ? nullptr : participant_factory_qos_name_.c_str();
    const char* p_qos_name = participant_qos_name_.empty() ? nullptr : participant_qos_name_.c_str();

    // 获取工厂
    factory_ = DDS::DomainParticipantFactory::get_instance_w_profile(
        qosFilePath, p_lib_name, p_prof_name, pf_qos_name);
    if (!factory_) {
        Logger::getInstance().error("[DDSManager_Bytes] 获取 DomainParticipantFactory 失败");
        return false;
    }

    // 创建 Participant
    participant_ = factory_->create_participant_with_qos_profile(
        domain_id_, p_lib_name, p_prof_name, p_qos_name, nullptr, DDS::STATUS_MASK_NONE);
    if (!participant_) {
        Logger::getInstance().error("[DDSManager_Bytes] 创建 DomainParticipant 失败");
        return false;
    }

    // 注册类型
    DDS::BytesTypeSupport* type_support = DDS::BytesTypeSupport::get_instance();
    if (!type_support) {
        Logger::getInstance().error("[DDSManager_Bytes] 获取 BytesTypeSupport 实例失败");
        return false;
    }

    const char* registered_type_name = type_support->get_type_name();
    if (!registered_type_name || strlen(registered_type_name) == 0) {
        Logger::getInstance().error("[DDSManager_Bytes] 类型名称为空");
        return false;
    }

    if (type_support->register_type(participant_, registered_type_name) != DDS::RETCODE_OK) {
        std::ostringstream oss;
        oss << "[DDSManager_Bytes] 注册类型 '" << registered_type_name << "' 失败";
        Logger::getInstance().error(oss.str());
        return false;
    }

    // 创建 Topic
    topic_ = participant_->create_topic(
        topic_name_.c_str(), registered_type_name,
        DDS::TOPIC_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    if (!topic_) {
        std::ostringstream oss;
        oss << "[DDSManager_Bytes] 创建 Topic '" << topic_name_ << "' 失败";
        Logger::getInstance().error(oss.str());
        return false;
    }

    // 创建 Writer 或 Reader
    if (role_ == "publisher") {
        data_writer_ = participant_->create_datawriter_with_topic_and_qos_profile(
            topic_->get_name(), type_support,
            "default_lib", "default_profile", data_writer_qos_name_.c_str(),
            nullptr, DDS::STATUS_MASK_NONE);
        if (!data_writer_) {
            Logger::getInstance().error("[DDSManager_Bytes] 创建 DataWriter 失败");
            return false;
        }
        Logger::getInstance().logAndPrint("[DDSManager_Bytes] DataWriter 创建成功");
    }
    else if (role_ == "subscriber") {
        void* mem = GloMemPool::allocate(sizeof(MyDataReaderListener), __FILE__, __LINE__);
        if (!mem) {
            Logger::getInstance().error("[DDSManager_Bytes] 分配监听器内存失败");
            return false;
        }
        listener_ = new (mem) MyDataReaderListener(std::move(dataCallback), std::move(endCallback));

        data_reader_ = participant_->create_datareader_with_topic_and_qos_profile(
            topic_->get_name(), type_support,
            "default_lib", "default_profile", data_reader_qos_name_.c_str(),
            listener_, DDS::STATUS_MASK_ALL);
        if (!data_reader_) {
            listener_->~MyDataReaderListener();
            GloMemPool::deallocate(listener_);
            listener_ = nullptr;
            Logger::getInstance().error("[DDSManager_Bytes] 创建 DataReader 失败");
            return false;
        }
        Logger::getInstance().logAndPrint("[DDSManager_Bytes] DataReader 创建成功");
    }
    else {
        Logger::getInstance().error("[DDSManager_Bytes] 无效角色: " + role_);
        return false;
    }

    is_initialized_ = true;
    Logger::getInstance().logAndPrint("[DDSManager_Bytes] 初始化成功");
    return true;
}

void DDSManager_Bytes::shutdown() {
    if (!factory_) return;

    if (listener_) {
        static_cast<MyDataReaderListener*>(listener_)->~MyDataReaderListener();
        GloMemPool::deallocate(listener_);
        listener_ = nullptr;
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
    Logger::getInstance().logAndPrint("[DDSManager_Bytes] 已关闭");
}

// 准备测试数据（带序列号和时间戳）
bool DDSManager_Bytes::prepareBytesData(
    DDS::Bytes& sample,
    int minSize,
    int maxSize,
    uint32_t sequence,
    uint64_t timestamp
) {
    int actualSize = minSize;
    if (minSize != maxSize) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(minSize, maxSize);
        actualSize = dis(gen);
    }

    // 确保至少能放下整个 Header
    const size_t header_size = sizeof(PacketHeader);
    if (actualSize < static_cast<int>(header_size)) {
        actualSize = header_size;
    }

    DDS_ULong ul_size = static_cast<DDS_ULong>(actualSize);
    DDS_ULong reserve_extra = (ul_size > 65536) ? 256 : (ul_size > 4096) ? 64 : 16;
    DDS_ULong alloc_size = ul_size + reserve_extra;

    DDS_Octet* buffer = static_cast<DDS_Octet*>(
        GloMemPool::allocate(alloc_size * sizeof(DDS_Octet), __FILE__, __LINE__)
        );
    if (!buffer) {
        Logger::getInstance().error("[DDSManager_Bytes] 内存分配失败，大小: " + std::to_string(alloc_size));
        return false;
    }

    DDS_OctetSeq_initialize(&sample.value);
    ZR_BOOLEAN loan_result = DDS_OctetSeq_loan_contiguous(&sample.value, buffer, ul_size, alloc_size);
    if (!loan_result) {
        GloMemPool::deallocate(buffer);
        DDS_OctetSeq_finalize(&sample.value);
        Logger::getInstance().error("[DDSManager_Bytes] 租借内存失败");
        return false;
    }

    // === 填充 PacketHeader ===
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(buffer);
    hdr->sequence = sequence;
    hdr->timestamp = timestamp;
    hdr->packet_type = 0;  // 正常数据包

    // === 填充 payload（可选）===
    for (DDS_ULong i = header_size; i < ul_size; ++i) {
        sample.value[i] = static_cast<DDS::Octet>((i + sequence) % 255);
    }

    sample.value._length = ul_size;

    std::ostringstream oss;
    oss << "prepareBytesData: seq=" << sequence
        << " ts=" << timestamp
        << " type=" << static_cast<int>(hdr->packet_type)
        << " length=" << ul_size;
    Logger::getInstance().logAndPrint(oss.str());

    return true;
}

// 清理 Bytes 数据
void DDSManager_Bytes::cleanupBytesData(DDS::Bytes& sample) {
    DDS_OctetSeq_finalize(&sample.value);
}

bool DDSManager_Bytes::prepareEndBytesData(DDS::Bytes& sample, int minSize) {
    DDS_ULong ul_size = static_cast<DDS_ULong>(minSize);
    const size_t header_size = sizeof(PacketHeader);
    if (ul_size < header_size) {
        ul_size = header_size;
    }

    const DDS_ULong reserve_extra = 16;
    DDS_ULong alloc_size = ul_size + reserve_extra;

    DDS_Octet* buffer = static_cast<DDS_Octet*>(
        GloMemPool::allocate(alloc_size * sizeof(DDS_Octet), __FILE__, __LINE__)
        );
    if (!buffer) {
        Logger::getInstance().error("[DDSManager_Bytes] 结束包内存分配失败，大小: " + std::to_string(alloc_size));
        return false;
    }

    DDS_OctetSeq_initialize(&sample.value);
    ZR_BOOLEAN loan_result = DDS_OctetSeq_loan_contiguous(&sample.value, buffer, ul_size, alloc_size);
    if (!loan_result) {
        GloMemPool::deallocate(buffer);
        DDS_OctetSeq_finalize(&sample.value);
        Logger::getInstance().error("[DDSManager_Bytes] 结束包租借内存失败");
        return false;
    }

    // === 构造结束包 Header ===
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(buffer);
    hdr->sequence = 0xFFFFFFFF;           // 可选标记
    hdr->timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    hdr->packet_type = 1;                 // 关键：标识这是结束包！

    // 剩余部分填充 0
    for (DDS_ULong i = header_size; i < ul_size; ++i) {
        sample.value[i] = 0;
    }
    sample.value._length = ul_size;

    Logger::getInstance().logAndPrint(
        "prepareEndBytesData: length=" + std::to_string(sample.value._length) +
        ", type=" + std::to_string(hdr->packet_type)
    );

    return true;
}