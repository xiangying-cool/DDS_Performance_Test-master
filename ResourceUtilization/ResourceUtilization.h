// ResourceUtilization.h
#pragma once

#include "SysMetrics.h" // ȷ������ SysMetrics.h
#include <memory>
#include <vector>

// --- ���������� PerCoreUsage ���������ͷ�ļ� ---
// ȷ���ڰ��� Windows.h ֮ǰ���� cstdint �Ի�ȡ��׼��������
// ���������Ŀ�����ط��Ѿ������� Windows.h����ȷ������˳����ȷ��
// ����������ֱ�Ӱ��� Windows.h ����ȡ DWORD
#ifdef _WIN32
#include <windows.h> // ���� DWORD �� Windows ����
#else
#include <cstdint>   // ����� Windows ƽ̨��ʹ�ñ�׼����
using DWORD = uint32_t; // ��ʾ����ʵ�ʿ�����Ҫ����ȷ��ӳ��
#endif
// --- �������� ---

// --- �������������ʹ���ʽṹ ---
struct PerCoreUsage {
    DWORD coreId;        // ���� ID
    double usagePercent; // ʹ���ʰٷֱ� (-1.0 ��ʾ����)

    // �޸����Ĭ�Ϲ��캯��
    PerCoreUsage() : coreId(0), usagePercent(-1.0) {}

    // �޸���Ĵ��������캯��
    PerCoreUsage(DWORD id, double usage) : coreId(id), usagePercent(usage) {}
};
// --- �������� ---

// ��Դ�����ʼ���� (����)
class ResourceUtilization {
public:
    // ��ȡ����ʵ��
    static ResourceUtilization& instance();

    // ��ʼ����Դ��� (������ʹ��ǰ����)
    bool initialize();

    // �ر���Դ��� (�������ǰ����)
    void shutdown();

    // �����Ľӿڡ��ɼ���ǰϵͳָ��
    // ����ֵ�������ϴε��ô˷���������¼���� CPU ʹ���ʷ�ֵ
    SysMetrics collectCurrentMetrics() const;

    // --- ���������ڿ��� CPU ��ʷ��¼�ķ��� ---
    // ���� CPU ʹ���ʼ�¼
    void start_cpu_recording();

    // ֹͣ CPU ��¼����ȡ��¼����ʷ����
    std::vector<float> stop_cpu_recording_and_get_history();
    // --- �������� ---

    // --- ��������ȡÿ�� CPU ����ʹ���ʵķ��� (����) ---
    bool initializePerCoreMonitoring();
    void shutdownPerCoreMonitoring();
    std::vector<PerCoreUsage> getPerCoreUsageSnapshot() const;
    // --- �������� ---

private:
    // ˽�й���/������������ֹ�ⲿʵ����
    ResourceUtilization();
    ~ResourceUtilization();

    // Pimpl (Pointer to Implementation) ģʽ
    // ����ƽ̨��ص�ʵ��ϸ��
    class Impl;
    std::unique_ptr<Impl> pimpl_;

    // ����Ƿ��ѳ�ʼ�� (�ƶ��� ResourceUtilization ����)
    bool is_initialized_ = false;
};

// ע�⣺SysMetrics.h �������������ϵͳ���ڴ�ָ���Ա�����磺
/*
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
    unsigned long long memory_alloc_count = 0;
    unsigned long long memory_dealloc_count = 0;
    long long memory_current_blocks = 0;

    // --- ������ϵͳ�������ڴ�ָ�� (��λ: KB) ---
    unsigned long long system_pagefile_usage_kb = 0;
    unsigned long long system_peak_pagefile_usage_kb = 0;
    unsigned long long system_working_set_kb = 0;
    unsigned long long system_peak_working_set_kb = 0;
    unsigned long long system_private_usage_kb = 0;
    unsigned long long system_quota_paged_pool_usage_kb = 0;
    unsigned long long system_quota_nonpaged_pool_usage_kb = 0;
    // --- �������� ---
};
*/