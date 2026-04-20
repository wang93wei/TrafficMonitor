#include "stdafx.h"
#include "GpuMemory.h"
#include <dxgi.h>

#pragma comment(lib, "DXGI.lib")

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

    bool GetGpuMemoryLimitFromDxgi(unsigned long long& limit)
    {
        IDXGIFactory1* p_factory{};
        if (FAILED(::CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&p_factory))) || p_factory == nullptr)
            return false;

        unsigned long long total_memory{};
        bool found{};
        for (UINT i{}; ; ++i)
        {
            IDXGIAdapter1* p_adapter{};
            HRESULT hr{ p_factory->EnumAdapters1(i, &p_adapter) };
            if (hr == DXGI_ERROR_NOT_FOUND)
                break;
            if (FAILED(hr) || p_adapter == nullptr)
            {
                if (p_adapter != nullptr)
                    p_adapter->Release();
                continue;
            }

            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(p_adapter->GetDesc1(&desc))
                && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0
                && desc.DedicatedVideoMemory > 0)
            {
                total_memory += static_cast<unsigned long long>(desc.DedicatedVideoMemory);
                found = true;
            }
            p_adapter->Release();
        }
        p_factory->Release();

        if (!found || total_memory == 0)
            return false;

        limit = total_memory;
        return true;
    }
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
    if (query.GetGpuMemoryLimit(limit) && limit > 0)
        return true;

    // 部分机器没有可用的 PDH 总量计数器，退回到显卡适配器自身的固定显存信息。
    return GetGpuMemoryLimitFromDxgi(limit);
}
