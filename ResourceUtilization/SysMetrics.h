#pragma once
#include <cstdint>

// ϵͳ��Դָ��ṹ��
struct SysMetrics {
    // CPU ʹ���ʷ�ֵ (�ٷֱ�)
    // -1.0 ��ʾ��ʼ��ʧ�ܻ�δ��ȡ������
    // >= 0.0 ��ʾ��Ч�ķ�ֵ�ٷֱ�
    double cpu_usage_percent_peak = -1.0;

    // �ڴ����ָ�� (��λ: KB) - ���� GloMemPool
    unsigned long long memory_peak_kb = 0;
    unsigned long long memory_current_kb = 0;

    // �ڴ����/�ͷ�ͳ�� - ���� GloMemPool
    unsigned long long memory_alloc_count = 0;
    unsigned long long memory_dealloc_count = 0;

    // ��ǰδ�ͷŵ��ڴ���� - ���� GloMemPool
    long long memory_current_blocks = 0;

    // --- ������ϵͳ�������ڴ�ָ�� (��λ: KB) ---
    // ע�⣺�� Windows API (psapi.h) ��ȡ��ͨ�����ֽڣ���Ҫת��Ϊ KB
    unsigned long long system_pagefile_usage_kb = 0;      // ҳ�ļ�ʹ���� (Commit Size)
    unsigned long long system_peak_pagefile_usage_kb = 0; // ��ֵҳ�ļ�ʹ���� (Peak Commit Size)
    unsigned long long system_working_set_kb = 0;         // ��������С (�����ڴ���ռ�õĴ�С)
    unsigned long long system_peak_working_set_kb = 0;    // ��ֵ��������С
    unsigned long long system_private_usage_kb = 0;       // ˽���ڴ�ʹ���� (ͨ���� Commit Size ��ͬ)
    unsigned long long system_quota_paged_pool_usage_kb = 0;    // ��ҳ�����ʹ����
    unsigned long long system_quota_nonpaged_pool_usage_kb = 0; // �Ƿ�ҳ�����ʹ����
    // --- �������� ---
};