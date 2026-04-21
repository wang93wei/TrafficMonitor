#pragma once
#include <Pdh.h>
#include <PdhMsg.h>

class CPdhQuery
{
public:
    CPdhQuery(LPCTSTR _fullCounterPath);
    virtual ~CPdhQuery();

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
