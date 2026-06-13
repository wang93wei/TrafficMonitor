#include "stdafx.h"
#include "DiskUsage.h"
#include <algorithm>
#include <cmath>

CPdhDiskUsage::CPdhDiskUsage()
    : CPdhQuery(_T("\\PhysicalDisk(*)\\% Idle Time"))
{
    m_isAvailable = Initialize();
    if (m_isAvailable)
    {
        // 预热：触发PDH内部初始化（首次QueryValues会填充实例列表）
        std::vector<CounterValueItem> dummy;
        QueryValues(dummy);
        ExtractDiskNames();
    }
}

CPdhDiskUsage::~CPdhDiskUsage()
{
}

void CPdhDiskUsage::ExtractDiskNames()
{
    m_diskNames.clear();
    std::vector<CounterValueItem> values;

    if (!QueryValues(values))
        return;

    for (const auto& item : values)
    {
        // 跳过 _Total 聚合实例，它不是具体物理磁盘，按 index 取值时会误命中导致返回全盘空闲反转为利用率。
        if (item.name == L"_Total")
            continue;
        CString name(item.name.c_str());
        m_diskNames.push_back(name);
    }
}

int CPdhDiskUsage::CalculateUtilization(double idleTime) const
{
    // 关键处理：NVMe/RAID等多队列磁盘的空闲时间可能 >100%
    // 例如: 4队列磁盘空闲时间=400% → 实际空闲=100% → 利用率=0%
    if (idleTime > 100.0)
        idleTime = 100.0;

    // 确保范围在0-100
    idleTime = (std::max)(0.0, (std::min)(100.0, idleTime));

    // 利用率 = 100% - 空闲时间
    double utilization = 100.0 - idleTime;
    return static_cast<int>(utilization + 0.5); // 四舍五入
}

bool CPdhDiskUsage::GetDiskUsage(int diskIndex, int& usage)
{
    usage = 0;
    if (!m_isAvailable)
        return false;

    // diskIndex 对应 m_diskNames 中的磁盘名（已排除 _Total）。
    if (diskIndex < 0 || diskIndex >= static_cast<int>(m_diskNames.size()))
        return false;
    std::wstring target_name = m_diskNames[diskIndex].GetString();

    std::vector<CounterValueItem> values;
    if (!QueryValues(values) || values.empty())
        return false;

    // 按名字匹配当前实例（PDH 实例顺序在热插拔/盘符变动时可能变化，不能假定与构造期一致）。
    for (const auto& item : values)
    {
        if (item.name == target_name)
        {
            usage = CalculateUtilization(item.value);
            return true;
        }
    }
    return false;

}

int CPdhDiskUsage::FindDiskIndex(const std::wstring diskName)
{
    int disk_index = -1;
    for (int i = 0; i < static_cast<int>(m_diskNames.size()); i++)
    {
        if (diskName == m_diskNames[i].GetString())
        {
            disk_index = i;
            break;
        }
    }
    return disk_index;
}
