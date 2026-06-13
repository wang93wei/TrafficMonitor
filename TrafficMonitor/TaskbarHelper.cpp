#include "stdafx.h"
#include "TaskbarHelper.h"
#include <algorithm>

// 存储显示器信息
struct MonitorInfo
{
    HMONITOR hMonitor;
    CRect rect;
};

// 存储任务栏信息
struct TaskbarInfo
{
    HWND hwnd;
    CRect rect;
};

std::vector<MonitorInfo> monitors;
std::vector<TaskbarInfo> taskbars;

// 枚举显示器的回调函数
static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    MonitorInfo info;
    info.hMonitor = hMonitor;
    info.rect = *lprcMonitor;
    if (!info.rect.IsRectEmpty())
        monitors.push_back(info);
    else
        ASSERT(FALSE);
    return TRUE;
}

// 枚举窗口的回调函数
static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    TCHAR className[256];
    GetClassName(hwnd, className, 256);

    // 检查是否是副显示器的任务栏
    if (_tcscmp(className, _T("Shell_SecondaryTrayWnd")) == 0)
    {
        TaskbarInfo info;
        info.hwnd = hwnd;
        GetWindowRect(hwnd, &info.rect);
        taskbars.push_back(info);
    }

    return TRUE;
}

// 判断任务栏中心点落在哪个显示器（返回该显示器在 monitors 中的索引，找不到返回 -1）。
// 用中心点而非左上角，避免负坐标显示器或任务栏贴边时对多个显示器同时判中的问题，
// 从而保证 std::sort 的比较器满足严格弱序（传递性、反对称性）。
static int FindMonitorIndexOfTaskbar(const TaskbarInfo& taskbar)
{
    CPoint center(taskbar.rect.left + taskbar.rect.Width() / 2,
                  taskbar.rect.top + taskbar.rect.Height() / 2);
    for (size_t i = 0; i < monitors.size(); ++i)
    {
        if (monitors[i].rect.PtInRect(center))
            return static_cast<int>(i);
    }
    return -1;
}

// 比较函数：按显示器枚举顺序排序任务栏（满足严格弱序）
static bool CompareTaskbarByMonitorOrder(const TaskbarInfo& a, const TaskbarInfo& b)
{
    int idx_a = FindMonitorIndexOfTaskbar(a);
    int idx_b = FindMonitorIndexOfTaskbar(b);

    // 先按显示器索引排序（找不到的视为在所有显示器之后，保证传递性）
    if (idx_a != idx_b)
        return idx_a < idx_b;

    // 同一显示器内（或都找不到），按 left 再按 top 排序，保证稳定的全序
    if (a.rect.left != b.rect.left)
        return a.rect.left < b.rect.left;
    if (a.rect.top != b.rect.top)
        return a.rect.top < b.rect.top;

    // 矩形完全相同，按 HWND 地址做最终决胜，确保严格弱序（a==b 时返回 false）
    return reinterpret_cast<uintptr_t>(a.hwnd) < reinterpret_cast<uintptr_t>(b.hwnd);
}

void CTaskbarHelper::GetAllSecondaryDisplayTaskbar(std::vector<HWND>& secondary_taskbars)
{
    monitors.clear();
    taskbars.clear();
    secondary_taskbars.clear();

    // 获取所有显示器信息
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);

    // 获取所有任务栏句柄
    EnumWindows(EnumWindowsProc, 0);

    // 按显示器顺序对任务栏排序
    std::sort(taskbars.begin(), taskbars.end(), CompareTaskbarByMonitorOrder);

    //保存任务栏句柄
    for (const auto& taskbar : taskbars)
    {
        secondary_taskbars.push_back(taskbar.hwnd);
    }
}

int CTaskbarHelper::GetDisplayNum()
{
    // 注意：本函数只枚举显示器。全局 taskbars 的残留由 GetAllSecondaryDisplayTaskbar
    // 在开头统一 clear 两者来保证一致性，这里无需处理 taskbars。
    monitors.clear();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
    return static_cast<int>(monitors.size());
}

int CTaskbarHelper::GetSecondaryTaskbarNum()
{
    // 同上，只枚举副屏任务栏。monitors 的残留由 GetAllSecondaryDisplayTaskbar 统一清理。
    taskbars.clear();
    EnumWindows(EnumWindowsProc, 0);
    return static_cast<int>(taskbars.size());
}
