// DDSManager_ZeroCopyBytes.h
#pragma once
#include <string>
#include <functional>

#include "ConfigData.h"
#include "DomainParticipant.h"
#include "DomainParticipantFactory.h"
#include "ZRBuiltinTypes.h"  

using OnDataReceivedCallback_ZC = std::function<void(const DDS_ZeroCopyBytes&, const DDS::SampleInfo&)>;
using OnEndOfRoundCallback = std::function<void()>;

class DDSManager_ZeroCopyBytes {
public:
    // 显式构造函数
    explicit DDSManager_ZeroCopyBytes(const ConfigData& config, const std::string& xml_qos_file_path);
    ~DDSManager_ZeroCopyBytes();

    // 禁止拷贝
    DDSManager_ZeroCopyBytes(const DDSManager_ZeroCopyBytes&) = delete;
    DDSManager_ZeroCopyBytes& operator=(const DDSManager_ZeroCopyBytes&) = delete;

    // 初始化 DDS 实体，传入回调（供外部测试模块使用）
    bool initialize(
        OnDataReceivedCallback_ZC dataCallback = nullptr,
        OnEndOfRoundCallback endCallback = nullptr
    );

    void shutdown();

    bool ensureBufferSize(size_t user_data_size);

    // 提供实体访问接口
    DDS::DomainParticipant* get_participant() const { return participant_; }
    DDS::DataWriter* get_data_writer() const { return data_writer_; }
    DDS::DataReader* get_data_reader() const { return data_reader_; }
    bool is_initialized() const { return is_initialized_; }

    // 辅助函数：准备 ZeroCopyBytes 测试数据
    // 注意：与 Bytes 不同，这里我们假设缓冲区已预分配，只需设置 userLength 并填充数据
    bool prepareZeroCopyData(DDS_ZeroCopyBytes& sample, int dataSize, uint32_t sequence);
    bool prepareEndZeroCopyData(DDS_ZeroCopyBytes& sample);

private:
    std::string xml_qos_file_path_;

    // 配置字段
    DDS::DomainId_t domain_id_;
    std::string topic_name_;
    std::string type_name_;
    std::string role_;
    std::string participant_factory_qos_name_;
    std::string participant_qos_name_;
    std::string data_writer_qos_name_;
    std::string data_reader_qos_name_;

    // 零拷贝专用配置
    static constexpr size_t DEFAULT_HEADER_RESERVE = 1024; // 推荐值，大于512即可
    size_t max_possible_size_; // 最大数据包大小，用于预分配
    char* global_buffer_;      // 预先分配的大块内存

    // DDS 实体
    DDS::DomainParticipantFactory* factory_ = nullptr;
    DDS::DomainParticipant* participant_ = nullptr;
    DDS::Topic* topic_ = nullptr;
    DDS::DataWriter* data_writer_ = nullptr;
    DDS::DataReader* data_reader_ = nullptr;

    class MyDataReaderListener;
    MyDataReaderListener* listener_ = nullptr;

    bool is_initialized_ = false;
};