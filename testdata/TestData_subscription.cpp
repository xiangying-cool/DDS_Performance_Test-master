#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Subscriber.h"
#include "DataReader.h"
#include "Topic.h"
#include "DataReaderListener.h"
#include "TestData.h"
#include "TestDataDataReader.h"
#include "TestDataTypeSupport.h"
#include <stdio.h>
#include <string.h>

// 继承DataReaderListener实现自定义的数据读者回调接口
class  Mylistener : public DDS::DataReaderListener
{
     // 数据到达回调函数
    void on_data_available(DDS::DataReader *the_reader)
    {
        printf("received data : \n");
        // 转化为类型安全的数据读者接口
        TestDataDataReader *exampleReader = dynamic_cast<TestDataDataReader*> (the_reader);
        TestDataSeq dataValues;
        DDS::SampleInfoSeq sampleInfos;
        DDS::ReturnCode_t rtn;
        if (exampleReader == NULL)
        {
            printf("datareader error\n");
            return;
        }
        rtn = exampleReader->take(
            dataValues,
            sampleInfos,
            100,
            DDS::ANY_SAMPLE_STATE,
            DDS::ANY_VIEW_STATE,
            DDS::ANY_INSTANCE_STATE);
        if (rtn != DDS::RETCODE_OK)
        {
            printf("take failed.\n");
            return;
        }
        for (unsigned int i = 0; i < sampleInfos.length(); i++)
        {
            // 在使用数据之前，应检查数据的有效性
            if (!sampleInfos[i].valid_data)
            {
                continue;
            }
            // TODO 在此处添加对数据的处理逻辑
            TestDataPrintData(&dataValues[i]);
        }
        exampleReader->return_loan(dataValues, sampleInfos);
        return;
    }
};
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
    // 监听器
    Mylistener *listener = new Mylistener;
    // 创建数据读者
    DDS::DataReader *exampleReader = participant->create_datareader_with_topic_and_qos_profile(
        "example",
        TestDataTypeSupport::get_instance(),
        "default_lib",
        "default_profile",
        "DataReaderReliability",
        listener,
        DDS::STATUS_MASK_ALL);
    if(exampleReader == NULL)
    {
        printf("create datareader failed.\n");
        getchar();
        return -1;
    }
    while(true)
    {
        ZRSleep(1000);
    }
    //回收DDS资源
    exampleReader->set_listener(NULL,  DDS::STATUS_MASK_NONE);
    delete listener;
    listener = NULL;
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
