// Logger.h
#pragma once
#include <string>
#include <memory>
#include <thread>

// ��־��¼���� (����ģʽ)
class Logger {
public:
    // ��ȡ����ʵ���ľ�̬����
    static Logger& getInstance() {
        static Logger instance; // �̰߳�ȫ�ľֲ���̬���� (C++11 ��)
        return instance;
    }

    // ��ֹ��������Ϳ�����ֵ
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // ��ʼ����־ϵͳ (ָ��Ŀ¼���ļ�ǰ׺����׺)
    bool initialize(
        const std::string& logDirectory,
        const std::string& filePrefix,
        const std::string& fileSuffix
    );

    // ��̬�������������ڷ�������õ���ʵ��
    static void setupLogger(
        const std::string& logDirectory,
        const std::string& filePrefix,
        const std::string& fileSuffix
    );

    // ��¼��־��Ϣ (����ӡ������̨)
    void log(const std::string& message);
    // ��¼������Ϣ
    void logConfig(const std::string& configInfo);
    // ��¼�����Ϣ
    void logResult(const std::string& result);
    // ��¼��־��Ϣ��ͬʱ��ӡ������̨
    void logAndPrint(const std::string& message);
    // ��¼ Info ������Ϣ
    void info(const std::string& msg);
    // ��¼ Error ������Ϣ
    void error(const std::string& msg);
    // �ر���־ϵͳ
    void close();

private:
    // ˽�й��캯����������������ֹ�ⲿʵ����
    Logger();
    ~Logger();

    // Pimpl (Pointer to Implementation) ģʽ
    // ������ʵ��ϸ�������� Impl ����
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};