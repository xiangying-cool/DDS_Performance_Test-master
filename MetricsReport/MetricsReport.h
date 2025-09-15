// MetricsReport.h
#pragma once

// --- ������Ҫ��ͷ�ļ� ---
#include "TestRoundResult.h" // ȷ�� TestRoundResult ���壨���� cpu_usage_history������
#include <vector>
#include <mutex>
// --- �������� ---

// ��Դ�����࣬�����ռ����洢�����ɲ����ִε���Դʹ��ժҪ
class MetricsReport {
public:
    // ���һ�ֲ��ԵĽ��
    void addResult(const TestRoundResult& result);

    // ���ɲ���ӡ���յĻ��ܱ���
    void generateSummary() const;

private:
    // �洢�����ִεĽ��
    std::vector<TestRoundResult> results_;
    // ���ڱ��� results_ �Ļ�����
    mutable std::mutex mtx_;
};