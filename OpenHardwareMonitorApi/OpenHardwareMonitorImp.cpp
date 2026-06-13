// 这是主 DLL 文件。

#include "stdafx.h"

#include "OpenHardwareMonitorImp.h"
#include <vector>
#include <mutex>

namespace OpenHardwareMonitorApi
{
    static std::wstring error_message;
    // error_message 被 GetHardwareInfo（定时器线程）写、GetErrorMessage（UI 线程）读，
    // std::wstring 非线程安全，必须用互斥量保护，否则并发读写会导致堆损坏/崩溃。
    static std::mutex error_message_mutex;

    //将CRL的String类型转换成C++的std::wstring类型
    static std::wstring ClrStringToStdWstring(System::String^ str)
    {
        if (str == nullptr)
        {
            return std::wstring();
        }
        else
        {
            const wchar_t* chars = (const wchar_t*)(Runtime::InteropServices::Marshal::StringToHGlobalUni(str)).ToPointer();
            std::wstring os = chars;
            Runtime::InteropServices::Marshal::FreeHGlobal(IntPtr((void*)chars));
            return os;
        }
    }

    // 安全地读取传感器数值。
    // ISensor::Value 是 Nullable<float>（即 float?），传感器未就绪时为 null，
    // 直接 Convert::ToDouble(nullptr) 会抛 NullReferenceException。这里判 HasValue，
    // null 时返回 false 让调用方跳过该传感器，避免异常和错误值。
    static bool SafeGetSensorValue(ISensor^ sensor, float& out_value)
    {
        if (sensor != nullptr && sensor->Value.HasValue)
        {
            out_value = sensor->Value.Value;
            return true;
        }
        return false;
    }


    std::shared_ptr<IOpenHardwareMonitor> CreateInstance()
    {
        std::shared_ptr<IOpenHardwareMonitor> pMonitor;
        try
        {
            MonitorGlobal::Instance()->Init();
            pMonitor = std::make_shared<COpenHardwareMonitor>();
        }
        catch (System::Exception^ e)
        {
            std::wstring msg = ClrStringToStdWstring(e->Message);
            std::lock_guard<std::mutex> lock(error_message_mutex);
            error_message = msg;
        }
        return pMonitor;
    }

    std::wstring GetErrorMessage()
    {
        std::lock_guard<std::mutex> lock(error_message_mutex);
        return error_message;
    }

    float COpenHardwareMonitor::CpuTemperature()
    {
        return m_cpu_temperature;
    }

    float COpenHardwareMonitor::GpuTemperature()
    {
        if (m_gpu_nvidia_temperature >= 0)
            return m_gpu_nvidia_temperature;
        else if (m_gpu_ati_temperature >= 0)
            return m_gpu_ati_temperature;
        else
            return m_gpu_intel_temperature;
    }

    float COpenHardwareMonitor::HDDTemperature()
    {
        return m_hdd_temperature;
    }

    float COpenHardwareMonitor::MainboardTemperature()
    {
        return m_main_board_temperature;
    }

    float COpenHardwareMonitor::GpuUsage()
    {
        if (m_gpu_nvidia_usage >= 0)
            return m_gpu_nvidia_usage;
        else if (m_gpu_ati_usage >= 0)
            return m_gpu_ati_usage;
        else
            return m_gpu_intel_usage;
    }

    float COpenHardwareMonitor::CpuFreq()
    {
            return m_cpu_freq;
    }

    float COpenHardwareMonitor::CpuUsage()
    {
        return m_cpu_usage;
    }

    const std::map<std::wstring, float>& COpenHardwareMonitor::AllHDDTemperature()
    {
        return m_all_hdd_temperature;
    }

    const std::map<std::wstring, float>& COpenHardwareMonitor::AllCpuTemperature()
    {
        return m_all_cpu_temperature;
    }

    const std::map<std::wstring, float>& COpenHardwareMonitor::AllHDDUsage()
    {
        return m_all_hdd_usage;
    }

    void COpenHardwareMonitor::SetCpuEnable(bool enable)
    {
        // CLR 冷启动期间（后台线程 Init 未完成）或 Init 失败时，computer 可能为 nullptr。
        // 访问 nullptr->IsCpuEnabled 会抛 NullReferenceException，从 native 栈逃逸导致进程崩溃。
        // C++/CLI 不会自动把托管异常转成 std::exception，必须显式 try/catch。
        try
        {
            auto computer = MonitorGlobal::Instance()->computer;
            if (computer != nullptr)
                computer->IsCpuEnabled = enable;
        }
        catch (System::Exception^) { /* 静默忽略，避免跨边界异常崩溃 */ }
    }

    void COpenHardwareMonitor::SetGpuEnable(bool enable)
    {
        try
        {
            auto computer = MonitorGlobal::Instance()->computer;
            if (computer != nullptr)
                computer->IsGpuEnabled = enable;
        }
        catch (System::Exception^) {}
    }

    void COpenHardwareMonitor::SetHddEnable(bool enable)
    {
        try
        {
            auto computer = MonitorGlobal::Instance()->computer;
            if (computer != nullptr)
                computer->IsStorageEnabled = enable;
        }
        catch (System::Exception^) {}
    }

    void COpenHardwareMonitor::SetMainboardEnable(bool enable)
    {
        try
        {
            auto computer = MonitorGlobal::Instance()->computer;
            if (computer != nullptr)
                computer->IsMotherboardEnabled = enable;
        }
        catch (System::Exception^) {}
    }

    bool COpenHardwareMonitor::GetCPUFreq(IHardware^ hardware, float& freq) {
        float max_clock_mhz = -1.0f;
        for (int i = 0; i < hardware->Sensors->Length; i++)
        {
            if (hardware->Sensors[i]->SensorType == SensorType::Clock)
            {
                String^ name = hardware->Sensors[i]->Name;
                // 只取 CPU 核心（LibreHardwareMonitor 命名为 "Core #N"）的时钟，
                // 跳过 "Bus Speed" 等非核心时钟。取所有核心时钟的最大值作为当前主频，
                // 而非把所有时钟算术平均（之前的实现会把内存/总线等无关时钟也算进去）。
                std::wstring wname = ClrStringToStdWstring(name);
                if (wname.rfind(L"Core", 0) == 0)
                {
                    float clock;
                    if (SafeGetSensorValue(hardware->Sensors[i], clock) && clock > max_clock_mhz)
                        max_clock_mhz = clock;
                }
            }
        }
        if (max_clock_mhz < 0)
            return false;     // 没有找到核心时钟传感器
        freq = max_clock_mhz / 1000.0f;
        return true;
    }

    bool COpenHardwareMonitor::GetCpuUsage(IHardware^ hardware, float& cpu_usage)
    {
        for (int i = 0; i < hardware->Sensors->Length; i++)
        {
            if (hardware->Sensors[i]->SensorType == SensorType::Load)
            {
                String^ name = hardware->Sensors[i]->Name;
                if (name == L"CPU Total")
                {
                    if (SafeGetSensorValue(hardware->Sensors[i], cpu_usage))
                        return true;
                }
            }
        }
        return false;
    }

    bool COpenHardwareMonitor::GetHardwareTemperature(IHardware^ hardware, float& temperature)
    {
        temperature = -1;
        std::vector<float> all_temperature;
        float core_temperature{ -1 };
        System::String^ temperature_name;
        switch (hardware->HardwareType)
        {
        case HardwareType::Cpu:
            temperature_name = L"Core Average";
            break;
        case HardwareType::GpuNvidia: case HardwareType::GpuAmd: case HardwareType::GpuIntel:
            temperature_name = L"GPU Core";
            break;
        default:
            break;
        }
        for (int i = 0; i < hardware->Sensors->Length; i++)
        {
            //找到温度传感器
            if (hardware->Sensors[i]->SensorType == SensorType::Temperature)
            {
                float cur_temperture;
                if (!SafeGetSensorValue(hardware->Sensors[i], cur_temperture))
                    continue;   // 传感器值未就绪，跳过
                all_temperature.push_back(cur_temperture);
                if (hardware->Sensors[i]->Name == temperature_name) //如果找到了名称为temperature_name的温度传感器，则将温度保存到core_temperature里
                    core_temperature = cur_temperture;
            }
        }
        if (core_temperature >= 0)
        {
            temperature = core_temperature;
            return true;
        }
        if (!all_temperature.empty())
        {
            //如果有多个温度传感器，则取平均值
            float sum{};
            for (auto i : all_temperature)
                sum += i;
            temperature = sum / all_temperature.size();
            return true;
        }
        //如果没有找到温度传感器，则在SubHardware中寻找
        for (int i = 0; i < hardware->SubHardware->Length; i++)
        {
            if (GetHardwareTemperature(hardware->SubHardware[i], temperature))
                return true;
        }
        return false;
    }

    bool COpenHardwareMonitor::GetCpuTemperature(IHardware^ hardware, float& temperature)
    {
        temperature = -1;
        m_all_cpu_temperature.clear();
        for (int i = 0; i < hardware->Sensors->Length; i++)
        {
            //找到温度传感器
            if (hardware->Sensors[i]->SensorType == SensorType::Temperature)
            {
                String^ name = hardware->Sensors[i]->Name;
                //保存每个CPU传感器的温度
                float temp_val;
                if (SafeGetSensorValue(hardware->Sensors[i], temp_val))
                    m_all_cpu_temperature[ClrStringToStdWstring(name)] = temp_val;
            }
        }
        //计算平均温度
        if (!m_all_cpu_temperature.empty())
        {
            float sum{};
            for (const auto& item : m_all_cpu_temperature)
                sum += item.second;
            temperature = sum / m_all_cpu_temperature.size();
        }
        return temperature > 0;
    }

    bool COpenHardwareMonitor::GetGpuUsage(IHardware^ hardware, float& gpu_usage)
    {
        float usage_max = 0;
        for (int i = 0; i < hardware->Sensors->Length; i++)
        {
            //找到负载
            if (hardware->Sensors[i]->SensorType == SensorType::Load)
            {
                float cur_gpu_usage;
                if (!SafeGetSensorValue(hardware->Sensors[i], cur_gpu_usage))
                    continue;   // 传感器值未就绪，跳过
                if (hardware->Sensors[i]->Name == L"GPU Core")
                {
                    gpu_usage = cur_gpu_usage;
                    return true;
                }

                //计算最大值
                if (cur_gpu_usage > usage_max)
                    usage_max = cur_gpu_usage;
            }
        }
        gpu_usage = usage_max;
        return true;
    }

    bool COpenHardwareMonitor::GetHddUsage(IHardware^ hardware, float& hdd_usage)
    {
        for (int i = 0; i < hardware->Sensors->Length; i++)
        {
            //找到负载
            if (hardware->Sensors[i]->SensorType == SensorType::Load)
            {
                if (hardware->Sensors[i]->Name == L"Total Activity")
                {
                    if (SafeGetSensorValue(hardware->Sensors[i], hdd_usage))
                        return true;
                }
            }
        }
        return false;
    }

    COpenHardwareMonitor::COpenHardwareMonitor()
    {
        ResetAllValues();
    }

    COpenHardwareMonitor::~COpenHardwareMonitor()
    {
        MonitorGlobal::Instance()->UnInit();
    }

    void COpenHardwareMonitor::ResetAllValues()
    {
        m_cpu_temperature = -1;
        m_gpu_nvidia_temperature = -1;
        m_gpu_ati_temperature = -1;
        m_gpu_intel_temperature = -1;
        m_hdd_temperature = -1;
        m_main_board_temperature = -1;
        m_gpu_nvidia_usage = -1;
        m_gpu_ati_usage = -1;
        m_gpu_intel_usage = -1;
        m_all_hdd_temperature.clear();
        m_all_hdd_usage.clear();
        m_cpu_freq = -1;
        m_cpu_usage = -1;
    }

    void COpenHardwareMonitor::InsertValueToMap(std::map<std::wstring, float>& value_map, const std::wstring& key, float value)
    {
        auto iter = value_map.find(key);
        if (iter == value_map.end())
        {
            value_map[key] = value;
        }
        else
        {
            std::wstring key_exist = iter->first;
            size_t index = key_exist.rfind(L'#');   //查找字符串是否含有#号
            if (index != std::wstring::npos)
            {
                //取到#号后面的数字，将其加1
                int num = _wtoi(key_exist.substr(index + 1).c_str());
                num++;
                key_exist = key_exist.substr(0, index + 1);
                key_exist += std::to_wstring(num);
            }
            else //没有#号则在末尾添加" #1"
            {
                key_exist += L" #1";
            }
            value_map[key_exist] = value;
        }
    }

    void COpenHardwareMonitor::GetHardwareInfo()
    {
        ResetAllValues();
        {
            std::lock_guard<std::mutex> lock(error_message_mutex);
            error_message.clear();
        }
        try
        {
            auto computer = MonitorGlobal::Instance()->computer;
            computer->Accept(MonitorGlobal::Instance()->updateVisitor);
            for (int i = 0; i < computer->Hardware->Count; i++)
            {
                //查找硬件类型
                switch (computer->Hardware[i]->HardwareType)
                {
                case HardwareType::Cpu:
                    if (m_cpu_temperature < 0)
                        GetCpuTemperature(computer->Hardware[i], m_cpu_temperature);
                    if (m_cpu_freq < 0)
                        GetCPUFreq(computer->Hardware[i], m_cpu_freq);
                    if (m_cpu_usage < 0)
                        GetCpuUsage(computer->Hardware[i], m_cpu_usage);
                    break;
                case HardwareType::GpuNvidia:
                    if (m_gpu_nvidia_temperature < 0)
                        GetHardwareTemperature(computer->Hardware[i], m_gpu_nvidia_temperature);
                    if (m_gpu_nvidia_usage < 0)
                        GetGpuUsage(computer->Hardware[i], m_gpu_nvidia_usage);
                    break;
                case HardwareType::GpuAmd:
                    if (m_gpu_ati_temperature < 0)
                        GetHardwareTemperature(computer->Hardware[i], m_gpu_ati_temperature);
                    if (m_gpu_ati_usage < 0)
                        GetGpuUsage(computer->Hardware[i], m_gpu_ati_usage);
                    break;
                case HardwareType::GpuIntel:
                    if (m_gpu_intel_temperature < 0)
                        GetHardwareTemperature(computer->Hardware[i], m_gpu_intel_temperature);
                    if (m_gpu_intel_usage < 0)
                        GetGpuUsage(computer->Hardware[i], m_gpu_intel_usage);
                    break;
                case HardwareType::Storage:
                {
                    float cur_hdd_temperature = -1;
                    GetHardwareTemperature(computer->Hardware[i], cur_hdd_temperature);
                    //m_all_hdd_temperature[ClrStringToStdWstring(computer->Hardware[i]->Name)] = cur_hdd_temperature;
                    InsertValueToMap(m_all_hdd_temperature, ClrStringToStdWstring(computer->Hardware[i]->Name), cur_hdd_temperature);
                    float cur_hdd_usage = -1;
                    GetHddUsage(computer->Hardware[i], cur_hdd_usage);
                    //m_all_hdd_usage[ClrStringToStdWstring(computer->Hardware[i]->Name)] = cur_hdd_usage;
                    InsertValueToMap(m_all_hdd_usage, ClrStringToStdWstring(computer->Hardware[i]->Name), cur_hdd_usage);
                    if (m_hdd_temperature < 0)
                        m_hdd_temperature = cur_hdd_temperature;
                }
                break;
                case HardwareType::Motherboard:
                    if (m_main_board_temperature < 0)
                        GetHardwareTemperature(computer->Hardware[i], m_main_board_temperature);
                    break;
                default:
                    break;
                }
            }
        }
        catch (System::Exception^ e)
        {
            std::wstring msg = ClrStringToStdWstring(e->Message);
            std::lock_guard<std::mutex> lock(error_message_mutex);
            error_message = msg;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////
    MonitorGlobal::MonitorGlobal()
    {

    }

    MonitorGlobal::~MonitorGlobal()
    {

    }

    void MonitorGlobal::Init()
    {
        updateVisitor = gcnew UpdateVisitor();
        computer = gcnew Computer();
        computer->Open();
    }

    void MonitorGlobal::UnInit()
    {
        computer->Close();
    }

}
