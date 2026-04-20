#include "stdafx.h"
#include "GpuMemory.h"
#include <dxgi.h>

#pragma comment(lib, "DXGI.lib")

namespace
{
    bool IsSameGpuAdapterLuid(const LUID& left, const LUID& right)
    {
        return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
    }

    bool ParseGpuAdapterLuid(const std::wstring& instance_name, LUID& luid)
    {
        unsigned int high_part{};
        unsigned int low_part{};
        if (swscanf_s(instance_name.c_str(), L"luid_0x%x_0x%x_phys_%*u", &high_part, &low_part) != 2)
            return false;

        luid.HighPart = static_cast<LONG>(high_part);
        luid.LowPart = static_cast<DWORD>(low_part);
        return true;
    }

    bool CollectGpuAdapterLuids(const std::vector<CPdhQuery::CounterValueItem>& valueItems, std::vector<LUID>& adapter_luids)
    {
        adapter_luids.clear();
        for (const auto& item : valueItems)
        {
            LUID luid{};
            if (!ParseGpuAdapterLuid(item.name, luid))
                continue;

            bool already_exists{};
            for (const auto& current_luid : adapter_luids)
            {
                if (IsSameGpuAdapterLuid(current_luid, luid))
                {
                    already_exists = true;
                    break;
                }
            }
            if (!already_exists)
                adapter_luids.push_back(luid);
        }
        return !adapter_luids.empty();
    }

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

    bool SumGpuAdapterMemoryCounterValues(const std::vector<CPdhQuery::CounterValueItem>& valueItems,
        const std::vector<LUID>& adapter_luids, unsigned long long& sum)
    {
        double total_value{};
        bool found{};
        for (const auto& item : valueItems)
        {
            if (item.name.empty() || item.value < 0)
                continue;

            LUID luid{};
            if (!ParseGpuAdapterLuid(item.name, luid))
                continue;

            for (const auto& current_luid : adapter_luids)
            {
                if (IsSameGpuAdapterLuid(current_luid, luid))
                {
                    total_value += item.value;
                    found = true;
                    break;
                }
            }
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

        bool QueryGpuMemoryLimitValues(std::vector<CounterValueItem>& valueItems)
        {
            if (!isInitialized)
                return false;

            return QueryValues(valueItems) && !valueItems.empty();
        }
    };

    bool GetGpuMemoryLimitFromDxgi(const std::vector<CPdhQuery::CounterValueItem>& valueItems, unsigned long long& limit)
    {
        std::vector<LUID> adapter_luids;
        if (!CollectGpuAdapterLuids(valueItems, adapter_luids))
            return false;

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
                for (const auto& luid : adapter_luids)
                {
                    if (IsSameGpuAdapterLuid(desc.AdapterLuid, luid))
                    {
                        total_memory += static_cast<unsigned long long>(desc.DedicatedVideoMemory);
                        found = true;
                        break;
                    }
                }
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
    std::vector<CounterValueItem> valueItems;
    if (!QueryValues(valueItems) || valueItems.empty())
        return false;

    std::vector<LUID> adapter_luids;
    CollectGpuAdapterLuids(valueItems, adapter_luids);

    static CPdhGPUMemoryLimitQuery query;
    std::vector<CounterValueItem> limitValueItems;
    if (query.QueryGpuMemoryLimitValues(limitValueItems))
    {
        if (!adapter_luids.empty())
        {
            if (SumGpuAdapterMemoryCounterValues(limitValueItems, adapter_luids, limit) && limit > 0)
                return true;
        }
        else
        {
            if (SumGpuAdapterMemoryCounterValues(limitValueItems, true, limit) && limit > 0)
                return true;
            if (SumGpuAdapterMemoryCounterValues(limitValueItems, false, limit) && limit > 0)
                return true;
        }
    }

    // 部分机器没有可用的 PDH 总量计数器，退回到与当前专用显存实例匹配的适配器固定显存信息。
    return GetGpuMemoryLimitFromDxgi(valueItems, limit);
}
