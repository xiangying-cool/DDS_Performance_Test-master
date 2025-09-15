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

// �̳�DataReaderListenerʵ���Զ�������ݶ��߻ص��ӿ�
class  Mylistener : public DDS::DataReaderListener
{
     // ���ݵ���ص�����
    void on_data_available(DDS::DataReader *the_reader)
    {
        printf("received data : \n");
        // ת��Ϊ���Ͱ�ȫ�����ݶ��߽ӿ�
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
            // ��ʹ������֮ǰ��Ӧ������ݵ���Ч��
            if (!sampleInfos[i].valid_data)
            {
                continue;
            }
            // TODO �ڴ˴���Ӷ����ݵĴ����߼�
            TestDataPrintData(&dataValues[i]);
        }
        exampleReader->return_loan(dataValues, sampleInfos);
        return;
    }
};
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
    // ������
    Mylistener *listener = new Mylistener;
    // �������ݶ���
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
    //����DDS��Դ
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
