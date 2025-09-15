// Main.h
#pragma once
#include <string>

#ifndef DDS_TRUE
#define DDS_TRUE  true
#endif

#ifndef DDS_FALSE
#define DDS_FALSE false
#endif

/**
 * @file Main.h
 * @brief ȫ�����úͳ������塣
 *
 * ��ͷ�ļ��������������ܲ��Թ�����ʹ�õ�ȫ��·����Ĭ��ֵ�ͳ�����
 * ͨ���޸Ĵ˴��Ķ��壬�������ɵص�����������в������������ڶ��Դ�ļ��в��Һ��滻��
 */

namespace GlobalConfig {

    // =========================
    // ·������
    // =========================

    /**
     * @brief Ĭ�ϵ����ܲ��������ļ� (JSON) ��·����
     * ����ļ������˾���Ĳ���������������������ɫ��QoS���Ƶȡ�
     */
    constexpr const char* DEFAULT_JSON_CONFIG_PATH = "..\\..\\..\\data\\zrdds_perf_config.json";

    /**
     * @brief Ĭ�ϵ� ZRDDS QoS �����ļ� (XML) ��·����
     * ����ļ����������п��õ� QoS ���ԣ���ɿ����䡢��ʷ��ȵȡ�
     * ·�������Ǿ���·��������ڿ�ִ���ļ�����Ŀ¼�����·����
     */
    constexpr const char* DEFAULT_QOS_XML_PATH = "..\\..\\..\\data\\zrdds_perf_test_qos.xml";

    /**
     * @brief �����־�ļ���Ĭ��Ŀ¼��
     * ����᳢���ڴ�Ŀ¼�´�����־�ļ���
     */
    constexpr const char* LOG_DIRECTORY = "..\\..\\..\\log";

    /**
     * @brief ��־�ļ�����ǰ׺��
     */
    constexpr const char* LOG_FILE_PREFIX = "log_zrdds_perf_bench_config_";

    /**
     * @brief ��־�ļ��ĺ�׺��
     */
    constexpr const char* LOG_FILE_SUFFIX = ".log";

    /*
	 * @brief Ĭ�ϵĽ�����Ŀ¼��
     * ����᳢���ڴ�Ŀ¼�´�������ļ���
     */
	constexpr const char* DEFAULT_RESULT_PATH = "..\\..\\..\\result";

} // namespace GlobalConfig
