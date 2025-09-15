#pragma once
#include "SysMetrics.h"

#include <vector>

struct TestRoundResult {
    int round_index;              // �ڼ���
    SysMetrics start_metrics;     // ��ʼʱ����Դ״̬
    SysMetrics end_metrics;       // ����ʱ����Դ״̬

    // ��ѡ���м�����㣨���ڻ�������ͼ��
    std::vector<SysMetrics> samples;

    // --- �������洢���ֲ��Ե� CPU ʹ������ʷ��¼ ---
    // ʹ�� float ���ܱ� double ��ʡһЩ�ڴ棬���ȶ� CPU % ͨ��Ҳ�㹻
    std::vector<float> cpu_usage_history;
    // --- �������� ---

    // ע�⣺���������������캯������Ҫȷ���ڹ���ʱ��ȷ��ʼ�� cpu_usage_history
    // �����Ƴ�����ʹ�þۺϳ�ʼ����Ĭ�Ϲ��캯����Ȼ���ֶ������ֶΡ�
    // ��ǰ������캯��û�г�ʼ�� samples �� cpu_usage_history��
    // Ϊ�˰�ȫ������������Ĭ�Ϲ��캯����ʹ�þۺϳ�ʼ����
    TestRoundResult() : round_index(0), start_metrics{}, end_metrics{} {
        // vector ��Ա���Զ�Ĭ�ϳ�ʼ��Ϊ��
    }

    TestRoundResult(int idx, const SysMetrics& start, const SysMetrics& end)
        : round_index(idx), start_metrics(start), end_metrics(end) {
        // ע�⣺������캯��û�г�ʼ�� samples �� cpu_usage_history vector��
        // ���ʹ�ô˹��캯������Ҫȷ���ں����������ǰ��vector �ǿյĻ�����ȷ����
        // �����ڴ˴���ʽ��ʼ����
        // samples = {};
        // cpu_usage_history = {};
    }
};