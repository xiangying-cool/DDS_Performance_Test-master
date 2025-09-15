// ConfigData.h
#pragma once
#include <string>
#include <vector>

struct ConfigData {
    std::string name;

    std::string m_dpfQosName;
    std::string m_dpQosName;
    std::string m_pubQosName;
    std::string m_subQosName;
    std::string m_writerQosName;
    std::string m_readerQosName;
    std::string m_typeName;
    std::string m_topicName;

    std::string m_clockDevName;
    std::string m_latencyMode;
    std::string m_resultPath;

    int m_activeLoop;
    int m_delayMode;
    int m_domainId;
    int m_loopNum;
    int m_remoteNum;
    int m_userAction;

    bool m_isPositive;
    bool m_logTimeStamp;
    bool m_checkSample;
    bool m_useDataArrived;
    bool m_useSyncDelay;
    bool m_useTaskNextSample;

    std::vector<std::string> configs;
    std::vector<int> m_domainIds;
    std::vector<int> m_minSize;
    std::vector<int> m_maxSize;
    std::vector<int> m_sendCount;
    std::vector<int> m_sendDelayCount;
    std::vector<int> m_sendDelay;
    std::vector<int> m_sendPrintGap;
    std::vector<int> m_recvPrintGap;

    std::vector<int> m_dpNum;
    std::vector<int> m_readerNum;
    std::vector<int> m_writerNum;
    std::vector<int> m_readerTopicRange;
    std::vector<int> m_writerTopicRange;

    // 标志字段：是否显式配置了该数组
    bool has_configs = false;
    bool has_m_domainIds = false;
    bool has_m_minSize = false;
    bool has_m_maxSize = false;
    bool has_m_sendCount = false;
    bool has_m_sendDelayCount = false;
    bool has_m_sendDelay = false;
    bool has_m_sendPrintGap = false;
    bool has_m_recvPrintGap = false;
    bool has_m_dpNum = false;
    bool has_m_readerNum = false;
    bool has_m_writerNum = false;
    bool has_m_readerTopicRange = false;
    bool has_m_writerTopicRange = false;
};