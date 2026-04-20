#include "stdafx.h"
#include "GpuMemory.h"

namespace
{
    bool SumGpuAdapterMemoryCounterValues(const std::vector<CPdhQuery::CounterValueItem>& valueItems,
        bool only_luid_items, unsigned long long& sum)
    {
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
    }

    class CPdhGPUMemoryLimitQuery : public CPdhQuery
    {
    public:
        CPdhGPUMemoryLimitQuery()
            : CPdhQuery(_T("\\GPU Adapter Memory(*)\\Dedicated Limit"))
        {
        }

        bool GetGpuMemoryLimit(unsigned long long& limit)
        {
            if (!isInitialized)
                return false;

            std::vector<CounterValueItem> valueItems;
            if (!QueryValues(valueItems) || valueItems.empty())
                return false;

            if (SumGpuAdapterMemoryCounterValues(valueItems, true, limit))
                return true;

            return SumGpuAdapterMemoryCounterValues(valueItems, false, limit);
        }
    };
}

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

    // 当前系统实例名通常形如 luid_0x<HIGH>_0x<LOW>_phys_0。
    // 若后续系统命名规则变化，则退回到累加所有非 _Total 实例。
    if (SumGpuAdapterMemoryCounterValues(valueItems, true, usage))
        return true;

    return SumGpuAdapterMemoryCounterValues(valueItems, false, usage);
}

bool CPdhGPUMemoryUsage::GetGpuMemoryLimit(unsigned long long& limit)
{
    static CPdhGPUMemoryLimitQuery query;
    return query.GetGpuMemoryLimit(limit);
}
