#include "stdafx.h"
#include "PdhQuery.h"

CPdhQuery::CPdhQuery(LPCTSTR _fullCounterPath)
    : fullCounterPath(_fullCounterPath)
{
    Initialize();
}

CPdhQuery::~CPdhQuery()
{
    //关闭查询（已初始化时才关闭，避免对未初始化/失败状态的句柄调用）
    if (query != nullptr)
    {
        PdhCloseQuery(query);
        query = nullptr;
        counter = nullptr;
    }
}

bool CPdhQuery::Initialize()
{
    if (isInitialized)
        return true;

    // 若之前 Initialize 部分成功（如 PdhOpenQuery 成功但后续失败）留下了句柄，
    // 重新进入时必须先释放，避免句柄泄漏。
    if (query != nullptr)
    {
        PdhCloseQuery(query);
        query = nullptr;
        counter = nullptr;
    }

    PDH_STATUS status;
    //打开查询
    status = PdhOpenQuery(NULL, NULL, &query);
    if (status != ERROR_SUCCESS)
    {
        query = nullptr;
        return false;
    }

    //添加计数器
    status = PdhAddCounter(query, fullCounterPath.GetString(), NULL, &counter);
    //先调用PdhAddCounter，如果失败使用PdhAddEnglishCounter再试一次
    if (status != ERROR_SUCCESS)
    {
        status = PdhAddEnglishCounter(query, fullCounterPath.GetString(), NULL, &counter);
        if (status != ERROR_SUCCESS)
        {
            PdhCloseQuery(query);
            query = nullptr;
            counter = nullptr;
            return false;
        }
    }

    //初始化计数器
    PdhCollectQueryData(query);
    isInitialized = true;
    return true;
}

bool CPdhQuery::QueryValue(double& value)
{
    if (!isInitialized)
        return false;

    //更新数据
    PdhCollectQueryData(query);
    PDH_FMT_COUNTERVALUE pdhValue;
    DWORD dwValue;
    PDH_STATUS status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, &dwValue, &pdhValue);
    if (status != ERROR_SUCCESS)
    {
        return false;
    }
    value = pdhValue.doubleValue;
    return true;
}

bool CPdhQuery::QueryValues(std::vector<CounterValueItem>& values)
{
    values.clear();
    if (!isInitialized)
        return false;

    //更新数据
    PdhCollectQueryData(query);
    DWORD dwBufferSize = 0;         // Size of the pItems buffer
    DWORD dwItemCount = 0;          // Number of items in the pItems buffer
    PDH_FMT_COUNTERVALUE_ITEM* pItems = NULL;
    PDH_STATUS status = PdhGetFormattedCounterArray(counter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems);
    if (PDH_MORE_DATA == status)
    {
        pItems = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(dwBufferSize);
        if (pItems)
        {
            status = PdhGetFormattedCounterArray(counter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems);
            if (ERROR_SUCCESS == status)
            {
                // Loop through the array and print the instance name and counter value.
                for (DWORD i = 0; i < dwItemCount; i++)
                {
                    CounterValueItem value_item;
                    // szName 在某些畸形实例下可能为 NULL，构造 wstring 时必须防御
                    value_item.name = (pItems[i].szName != nullptr) ? pItems[i].szName : L"";
                    value_item.value = pItems[i].FmtValue.doubleValue;
                    values.push_back(value_item);
                }
            }
            else
            {
                // 失败路径必须先释放已分配的缓冲，否则内存泄漏
                free(pItems);
                return false;
            }

            free(pItems);
            pItems = NULL;
            dwBufferSize = dwItemCount = 0;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}
