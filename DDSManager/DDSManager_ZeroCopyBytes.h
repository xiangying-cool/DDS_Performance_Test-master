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
    // ��ʽ���캯��
    explicit DDSManager_ZeroCopyBytes(const ConfigData& config, const std::string& xml_qos_file_path);
    ~DDSManager_ZeroCopyBytes();

    // ��ֹ����
    DDSManager_ZeroCopyBytes(const DDSManager_ZeroCopyBytes&) = delete;
    DDSManager_ZeroCopyBytes& operator=(const DDSManager_ZeroCopyBytes&) = delete;

    // ��ʼ�� DDS ʵ�壬����ص������ⲿ����ģ��ʹ�ã�
    bool initialize(
        OnDataReceivedCallback_ZC dataCallback = nullptr,
        OnEndOfRoundCallback endCallback = nullptr
    );

    void shutdown();

    bool ensureBufferSize(size_t user_data_size);

    // �ṩʵ����ʽӿ�
    DDS::DomainParticipant* get_participant() const { return participant_; }
    DDS::DataWriter* get_data_writer() const { return data_writer_; }
    DDS::DataReader* get_data_reader() const { return data_reader_; }
    bool is_initialized() const { return is_initialized_; }

    // ����������׼�� ZeroCopyBytes ��������
    // ע�⣺�� Bytes ��ͬ���������Ǽ��軺������Ԥ���䣬ֻ������ userLength ���������
    bool prepareZeroCopyData(DDS_ZeroCopyBytes& sample, int dataSize, uint32_t sequence);
    bool prepareEndZeroCopyData(DDS_ZeroCopyBytes& sample);

private:
    std::string xml_qos_file_path_;

    // �����ֶ�
    DDS::DomainId_t domain_id_;
    std::string topic_name_;
    std::string type_name_;
    std::string role_;
    std::string participant_factory_qos_name_;
    std::string participant_qos_name_;
    std::string data_writer_qos_name_;
    std::string data_reader_qos_name_;

    // �㿽��ר������
    static constexpr size_t DEFAULT_HEADER_RESERVE = 1024; // �Ƽ�ֵ������512����
    size_t max_possible_size_; // ������ݰ���С������Ԥ����
    char* global_buffer_;      // Ԥ�ȷ���Ĵ���ڴ�

    // DDS ʵ��
    DDS::DomainParticipantFactory* factory_ = nullptr;
    DDS::DomainParticipant* participant_ = nullptr;
    DDS::Topic* topic_ = nullptr;
    DDS::DataWriter* data_writer_ = nullptr;
    DDS::DataReader* data_reader_ = nullptr;

    class MyDataReaderListener;
    MyDataReaderListener* listener_ = nullptr;

    bool is_initialized_ = false;
};