#pragma once
#include "PdhQuery.h"

// GPU显存占用监控类（专用显存）
class CPdhGPUMemoryUsage : public CPdhQuery
{
public:
    CPdhGPUMemoryUsage();
    ~CPdhGPUMemoryUsage();

    // 获取专用显存占用（单位：字节）
    // 返回值: true=成功, false=失败
    bool GetGpuMemoryUsage(/*out*/ unsigned long long& usage);
};
