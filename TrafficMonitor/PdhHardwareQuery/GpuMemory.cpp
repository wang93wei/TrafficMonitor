#include "stdafx.h"
#include "GpuMemory.h"

///////////////////////////////////////////////////////////////////////////////////////////
// CPdhGPUMemoryUsage 实现
///////////////////////////////////////////////////////////////////////////////////////////
CPdhGPUMemoryUsage::CPdhGPUMemoryUsage()
    : CPdhQuery(_T("\\GPU Adapter Memory(*)\\Dedicated Usage"))
{
}

CPdhGPUMemoryUsage::~CPdhGPUMemoryUsage()
{
}

bool CPdhGPUMemoryUsage::GetGpuMemoryUsage(unsigned long long& usage)
{
    if (!isInitialized)
        return false;

    std::vector<CounterValueItem> valueItems;
    if (!QueryValues(valueItems) || valueItems.empty())
        return false;

    auto sumValues = [&](bool only_luid_items, unsigned long long& sum) {
        double total_value{};
        bool found{};
        for (const auto& item : valueItems)
        {
            if (item.name.empty())
                continue;
            if (only_luid_items && item.name.rfind(L"luid_", 0) != 0)
                continue;
            if (!only_luid_items && item.name == L"_Total")
                continue;
            if (item.value < 0)
                continue;
            total_value += item.value;
            found = true;
        }
        if (!found)
            return false;
        sum = static_cast<unsigned long long>(total_value + 0.5);
        return true;
    };

    // 当前系统实例名通常形如 luid_0x<HIGH>_0x<LOW>_phys_0。
    // 若后续系统命名规则变化，则退回到累加所有非 _Total 实例。
    if (sumValues(true, usage))
        return true;

    return sumValues(false, usage);
}
