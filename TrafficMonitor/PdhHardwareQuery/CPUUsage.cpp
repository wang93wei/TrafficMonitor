#include "stdafx.h"
#include "CPUUsage.h"
#include "Common.h"
#include "TrafficMonitor.h"
#include <powerbase.h>
#include <sysinfoapi.h>


///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
CPdhCPUUsage::CPdhCPUUsage()
    : CPdhQuery(theApp.m_win_version.GetMajorVersion() >= 10 ? _T("\\Processor Information(_Total)\\% Processor Utility") : _T("\\Processor Information(_Total)\\% Processor Time"))
{
}

bool CPdhCPUUsage::GetCPUUsage(int& cpu_usage)
{
    double value{};
    if (QueryValue(value))
    {
        cpu_usage = static_cast<int>(value);
        if (cpu_usage > 100)
            cpu_usage = 100;
        // PDH 首次采样或异常时可能返回负值，需钳制下界，避免显示负利用率
        if (cpu_usage < 0)
            cpu_usage = 0;
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
CCPUUsage::CCPUUsage()
{
}

int CCPUUsage::GetCpuUsage(bool use_cpu_time)
{
    int cpu_usage{};
    if (use_cpu_time)
    {
        cpu_usage = GetCpuUsageByGetSystemTimes();
    }
    else
    {
        //如果通过pdh获取CPU利用率失败，采用GetSystemTimes获取
        if (!m_pdh_cup_usage_query.GetCPUUsage(cpu_usage))
        {
            cpu_usage = GetCpuUsageByGetSystemTimes();
            //写入日志
            //static bool write_log = false;
            //if (!write_log)
            //{
            //    CString str_log = CCommon::LoadTextFormat(IDS_GET_CPU_USAGE_BY_PDH_FAILED_LOG, { fullCounterPath });
            //    CCommon::WriteLog(str_log, theApp.m_log_path.c_str());
            //    write_log = true;
            //}
        }
    }
    return cpu_usage;
}

int CCPUUsage::GetCpuUsageByGetSystemTimes()
{
    int cpu_usage{};
    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    GetSystemTimes(&idleTime, &kernelTime, &userTime);

    __int64 idle = CCommon::CompareFileTime2(m_preidleTime, idleTime);
    __int64 kernel = CCommon::CompareFileTime2(m_prekernelTime, kernelTime);
    __int64 user = CCommon::CompareFileTime2(m_preuserTime, userTime);

    if (kernel + user == 0)
    {
        cpu_usage = 0;
    }
    else
    {
        //（总的时间-空闲时间）/总的时间=占用cpu的时间就是使用率。
        // 用浮点计算避免整数除法丢精度；休眠唤醒/时间回绕时 idle 可能 > kernel+user 导致负值，
        // 不应再用 abs 掩盖（会得到错误的正值），而应 clamp 到 [0,100]。
        double usage = (kernel + user - idle) * 100.0 / (kernel + user);
        if (usage < 0)
            usage = 0;
        else if (usage > 100)
            usage = 100;
        cpu_usage = static_cast<int>(usage + 0.5);     // 四舍五入
    }
    m_preidleTime = idleTime;
    m_prekernelTime = kernelTime;
    m_preuserTime = userTime;

    return cpu_usage;
}
