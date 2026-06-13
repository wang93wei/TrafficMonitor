#include "stdafx.h"
#include "WindowsWebExperienceDetector.h"
#ifndef DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Storage.h>
#pragma comment(lib, "RuntimeObject.lib")
#endif
#include "TrafficMonitor.h"
#include <thread>
#include <atomic>
#include <memory>

bool WindowsWebExperienceDetector::IsDetected() noexcept
{
#ifndef DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR
    if (theApp.m_win_version.IsWindows11OrLater())
    {
        // FindPackagesForUser 依赖 AppX 服务(AppXSVC)，该服务未启动或卡顿时可能阻塞数十秒，
        // 直接在任务栏窗口初始化(UI 线程)同步调用会导致界面长时间无响应(用户看到黑块)。
        // 改为后台线程执行 + 超时保护：超过 5 秒视为未检测到(降级为 Win11 无 Web Experience 处理)。
        // 注意：不能用 std::async，因为其返回的 future 析构会阻塞等待线程结束，违背超时目的。
        // 这里用 detached 线程 + 原子结果变量，超时后线程继续运行自行结束，不阻塞 UI。
        auto p_result = std::make_shared<std::atomic<int>>(-1);    // -1=未完成, 0=未检测到, 1=已检测到
        std::thread([p_result]() {
            try
            {
                auto manager = winrt::Windows::Management::Deployment::PackageManager{};
                auto packages = manager.FindPackagesForUser(
                    {},
                    winrt::hstring{ L"MicrosoftWindows.Client.WebExperience" },
                    winrt::hstring{
                        L"CN=Microsoft Windows, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" });
                p_result->store(packages.First().HasCurrent() ? 1 : 0, std::memory_order_relaxed);
            }
            catch (...)
            {
                p_result->store(0, std::memory_order_relaxed);
            }
        }).detach();

        // 轮询等待，最多 5 秒
        for (int i = 0; i < 50; ++i)
        {
            int val = p_result->load(std::memory_order_relaxed);
            if (val >= 0)
                return val != 0;
            Sleep(100);
        }
        return false;   // 超时
    }
#endif
    return false;
}
