// DDSManager_Bytes.h
#pragma once

#include "ConfigData.h"
#include "ZRBuiltinTypes.h"
#include "ZRDDSDataReader.h"
#include "ZRDDSDataWriter.h"
#include "DomainParticipant.h"
#include "DomainParticipantFactory.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

class DDSManager_Bytes {
public:
    using OnDataReceivedCallback_Bytes = std::function<void(const DDS::Bytes&, const DDS::SampleInfo&)>;
    using OnEndOfRoundCallback = std::function<void()>;

    DDSManager_Bytes(const ConfigData& config, const std::string& xml_qos_file_path);
    ~DDSManager_Bytes();

    // 初始化 DDS 实体，传入回调（供外部测试模块使用）
    bool initialize(
        OnDataReceivedCallback_Bytes dataCallback = nullptr,
        OnEndOfRoundCallback endCallback = nullptr
    );

    void shutdown();

    // 获取实体指针
    DDS::DataWriter* get_data_writer() const { return data_writer_; }
    DDS::DataReader* get_data_reader() const { return data_reader_; }

    // 准备测试数据（带序列号和时间戳）
    bool prepareBytesData(
        DDS::Bytes& sample,
        int minSize,
        int maxSize,
        uint32_t sequence,
        uint64_t timestamp
    );
    bool prepareEndBytesData(DDS_Bytes& sample, int minSize);

    // 清理 Bytes 数据
    void cleanupBytesData(DDS::Bytes& sample);

private:
    // 配置参数
    int domain_id_;
    std::string topic_name_;
    std::string type_name_;
    std::string role_;
    std::string participant_factory_qos_name_;
    std::string participant_qos_name_;
    std::string data_writer_qos_name_;
    std::string data_reader_qos_name_;
    std::string xml_qos_file_path_;

    // DDS 实体
    DDS::DomainParticipantFactory* factory_ = nullptr;
    DDS::DomainParticipant* participant_ = nullptr;
    DDS::Topic* topic_ = nullptr;
    DDS::DataWriter* data_writer_ = nullptr;
    DDS::DataReader* data_reader_ = nullptr;
    class MyDataReaderListener;
    MyDataReaderListener* listener_ = nullptr;

    bool is_initialized_ = false;

    // 内部 Listener 类声明
    class MyDataReaderListener;
};
