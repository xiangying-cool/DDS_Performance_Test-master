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
    // 设置域号
    DDS::DomainId_t domainId = 1;
    // 获取参与者工厂实例
    DDS::DomainParticipantFactory* factory = DDS::DomainParticipantFactory::get_instance_w_profile(
        NULL,"default_lib","default_profile","ExampleApp");
    if (factory == NULL)
    {
        getchar();
        return -1;
    }
    // 域参与者
    DDS::DomainParticipantQos dpQos;
    factory->get_default_participant_qos(dpQos);
    // TODO 在这里修改DomainParticipantQos
    // 创建域参与者
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
    // 创建数据写者 
    DDS::DataWriter *writer = participant->create_datawriter_with_topic_and_qos_profile(
        "example",
        TestDataTypeSupport::get_instance(),
        "default_lib",
        "default_profile",
        "DataWriterReliability",
        NULL,
        DDS::STATUS_MASK_NONE);
    // 转化为类型相关的数据写者
    TestDataDataWriter *exampleWriter = dynamic_cast<TestDataDataWriter*> (writer);
    if (exampleWriter == NULL)
    {
        printf("create datawriter failed.\n");
        getchar();
        return -1;
    }
    // 创建数据样本
    TestData data;
    TestDataInitialize(&data);
    //仅对有key的数据调用以下函数
    DDS::InstanceHandle_t handle = exampleWriter->register_instance(data);
    //发送数据
    while (true)
    {
        //TODO 在此处修改样本值 
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
    //回收DDS资源
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