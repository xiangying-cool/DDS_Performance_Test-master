#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Publisher.h"
#include "DataWriter.h"
#include "TestDataTypeSupport.h"
#include "TestDataDataWriter.h"
#include "TestData.h"
#include "ZRSleep.h"
#include <stdio.h>
#include <string.h>

int main()
{
    // �������
    DDS::DomainId_t domainId = 1;
    // ��ȡ�����߹���ʵ��
    DDS::DomainParticipantFactory* factory = DDS::DomainParticipantFactory::get_instance_w_profile(
        NULL,"default_lib","default_profile","ExampleApp");
    if (factory == NULL)
    {
        getchar();
        return -1;
    }
    // �������
    DDS::DomainParticipantQos dpQos;
    factory->get_default_participant_qos(dpQos);
    // TODO �������޸�DomainParticipantQos
    // �����������
    DDS::DomainParticipant *participant = factory->create_participant_with_qos_profile(
        domainId,
        "default_lib",
        "default_profile",
        "udp_dp", 
        NULL,
        DDS::STATUS_MASK_NONE);
    if (participant == NULL)
    {
        printf("create participant failed.\n");
        getchar();
        return -1;
    }
    // ��������д�� 
    DDS::DataWriter *writer = participant->create_datawriter_with_topic_and_qos_profile(
        "example",
        TestDataTypeSupport::get_instance(),
        "default_lib",
        "default_profile",
        "DataWriterReliability",
        NULL,
        DDS::STATUS_MASK_NONE);
    // ת��Ϊ������ص�����д��
    TestDataDataWriter *exampleWriter = dynamic_cast<TestDataDataWriter*> (writer);
    if (exampleWriter == NULL)
    {
        printf("create datawriter failed.\n");
        getchar();
        return -1;
    }
    // ������������
    TestData data;
    TestDataInitialize(&data);
    //������key�����ݵ������º���
    DDS::InstanceHandle_t handle = exampleWriter->register_instance(data);
    //��������
    while (true)
    {
        //TODO �ڴ˴��޸�����ֵ 
        data.value.ensure_length(1, 1);
        data.value[0] = 0xAA;
        DDS::ReturnCode_t rtn = exampleWriter->write(data, handle);
        if (rtn != DDS::RETCODE_OK)
        {
            printf("write data failed.");
            continue;
        }
        printf("send a data\n");
        ZRSleep(1000);
    }
    TestDataFinalize(&data);
    //����DDS��Դ
    if(participant->delete_contained_entities() != DDS::RETCODE_OK)
    {
        printf("DomainParticipant delete contained entities failed");
        getchar();
        return -1;
    }
    if(DDS::DomainParticipantFactory::get_instance()->delete_participant(participant) != DDS::RETCODE_OK)
    {
        printf("DomainParticipantFactory delete DomainParticipant failed");
        getchar();
        return -1;
    }
    if(DDS::DomainParticipantFactory::get_instance()->finalize_instance() != DDS::RETCODE_OK)
    {
        printf("DomainParticipantFactory finalize instance failed");
        getchar();
        return -1;
    }
    return 0;
}