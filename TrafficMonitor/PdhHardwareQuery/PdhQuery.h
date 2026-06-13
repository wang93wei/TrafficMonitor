#pragma once
#include <Pdh.h>
#include <PdhMsg.h>

class CPdhQuery
{
public:
    CPdhQuery(LPCTSTR _fullCounterPath);
    virtual ~CPdhQuery();

    // CPdhQuery 持有 PDH 句柄（HQUERY/HCOUNTER）等裸系统资源。
    // 拷贝会导致两个对象共享同一句柄，析构时 PdhCloseQuery 双调用。
    // 这里显式禁用拷贝与拷贝赋值以彻底杜绝该风险。
    CPdhQuery(const CPdhQuery&) = delete;
    CPdhQuery& operator=(const CPdhQuery&) = delete;

protected:
    bool Initialize();
    bool QueryValue(double& value);

public:
    struct CounterValueItem
    {
        std::wstring name;
        double value{};
    };

protected:
    bool QueryValues(std::vector<CounterValueItem>& values);

protected:
    HQUERY query = nullptr;
    HCOUNTER counter = nullptr;
    bool isInitialized = false;
    CString fullCounterPath;
};
