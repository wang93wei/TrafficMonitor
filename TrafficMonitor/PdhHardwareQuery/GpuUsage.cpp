#include "stdafx.h"
#include "GpuUsage.h"

///////////////////////////////////////////////////////////////////////////////////////////
// CPdhGPUUsage 实现
///////////////////////////////////////////////////////////////////////////////////////////
CPdhGPUUsage::CPdhGPUUsage()
: CPdhQuery(_T("\\GPU Engine(*)\\Utilization Percentage"))
{
}

CPdhGPUUsage::~CPdhGPUUsage()
{
}

bool CPdhGPUUsage::GetGpuUsage(int& usage)
{
    if (isInitialized)
    {
        std::vector<CounterValueItem> valueItems;
        if (QueryValues(valueItems))
        {
            if (!valueItems.empty())
            {
                // 按引擎类型（实例名最后一段，如 3D/Compute/Copy/VideoDecode）分组累加利用率，
                // 取所有类型中的最大值作为显示利用率。
                // 注意：这与 Windows 任务管理器的算法（对所有引擎实例求和再 cap 100）并不完全相同，
                // 此处取最忙引擎类型是为了避免不同引擎类型（如 3D + 视频解码）叠加导致数值偏高。
                std::map<std::wstring, double> gpu_usage_map;
                for (const auto& item : valueItems)
                {
                    // 跳过 _Total 聚合实例，它不是具体引擎
                    if (item.name == L"_Total")
                        continue;
                    // PDH 异常时可能返回负值，过滤掉避免累加出错误的最大值
                    if (item.value < 0)
                        continue;
                    std::wstring item_name = item.name;
                    size_t index = item.name.rfind(L'_');
                    if (index != std::wstring::npos)
                        item_name = item.name.substr(index + 1);
                    gpu_usage_map[item_name] += item.value;
                }
                //查找所有类型中最大的值作为总利用率
                double max_value = 0;
                for (const auto& item : gpu_usage_map)
                {
                    if (item.second > max_value)
                        max_value = item.second;
                }

                usage = static_cast<int>(max_value + 0.5);  // 四舍五入
                usage = min(max(usage, 0), 100);        // 限制在0-100范围
                return true;
            }
        }
    }

    return false;
}
