# TrafficMonitor 项目上下文

## 项目概述

TrafficMonitor 是一款 Windows 平台的网速监控悬浮窗软件，使用 C++/MFC 开发。主要功能包括：
- 实时显示网速、CPU及内存利用率
- 支持嵌入任务栏显示
- 支持更换皮肤和自定义皮肤
- 历史流量统计
- 硬件信息监控（温度、显卡利用率、硬盘利用率等）
- 插件系统扩展

**当前版本**: 1.85.1

## 技术栈

- **语言**: C++20
- **框架**: MFC (Microsoft Foundation Classes)
- **构建工具**: Visual Studio 2022 (MSBuild)
- **平台工具集**: v143
- **字符集**: Unicode
- **Windows SDK**: 10.0

## 项目结构

```
TrafficMonitor/
├── TrafficMonitor/           # 主程序源码
│   ├── TrafficMonitor.h      # 应用程序主类 (CTrafficMonitorApp)
│   ├── TrafficMonitorDlg.h   # 主对话框
│   ├── Common.h              # 通用工具函数
│   ├── CommonData.h          # 公共数据结构定义
│   ├── stdafx.h              # 预编译头
│   └── ...                   # 其他源文件
├── OpenHardwareMonitorApi/   # 硬件监控 API 库
│   └── OpenHardwareMonitorImp.cpp/h
├── PluginDemo/               # 插件开发示例
│   └── PluginInterface.h
├── include/                  # 公共头文件
│   └── PluginInterface.h     # 插件接口定义
├── .github/workflows/        # GitHub Actions CI
└── TrafficMonitor.sln        # Visual Studio 解决方案
```

## 构建指南

### 使用 Visual Studio 构建

1. 使用 Visual Studio 2022 打开 `TrafficMonitor.sln`
2. 选择构建配置和平台
3. 构建解决方案

### 使用命令行构建

#### 方法一：使用 Developer Command Prompt（推荐）

打开 "Developer Command Prompt for VS 2022" 或 "x64 Native Tools Command Prompt for VS 2022"，然后执行：

```powershell
# x64 Release 版本
msbuild -p:configuration=release -p:platform=x64

# x86 Release 版本
msbuild -p:configuration=release -p:platform=x86

# ARM64EC Release 版本
msbuild -p:configuration=release -p:platform=ARM64EC

# Lite 版本（无温度监控，不需要管理员权限）
msbuild -p:configuration="Release (lite)" -p:platform=x64
```

#### 方法二：使用 MSBuild 完整路径

如果 MSBuild 未添加到 PATH，可以使用完整路径：

```powershell
# 设置 MSBuild 路径（根据实际安装位置调整）
$MSBuild = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"

# x64 Release 版本
& $MSBuild -p:configuration=release -p:platform=x64

# x86 Release 版本
& $MSBuild -p:configuration=release -p:platform=x86

# Lite 版本
& $MSBuild -p:configuration="Release (lite)" -p:platform=x64
```

常见 MSBuild 安装位置：
- VS 2022 BuildTools: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe`
- VS 2022 Community: `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`
- VS 2022 Professional: `C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe`
- VS 2022 Enterprise: `C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe`

### 构建配置说明

| 配置 | 说明 | 管理员权限 | 温度监控 |
|------|------|------------|----------|
| Release/Debug | 标准版 | 需要 | 包含 |
| Release (lite)/Debug (lite) | Lite版 | 不需要 | 不包含 |

### 输出目录

- x86: `Bin/Release/` 或 `Bin/Debug/`
- x64: `Bin/x64/Release/` 或 `Bin/x64/Debug/`
- ARM64EC: `Bin/ARM64EC/Release/`

## 核心架构

### 应用程序类 (CTrafficMonitorApp)

继承自 `CWinApp` 和 `ITrafficMonitor`，负责：
- 应用程序初始化和退出
- 配置文件读写
- 监控数据管理（网速、CPU、内存、温度等）
- 插件管理
- DPI 感知

关键成员变量：
- `m_in_speed`, `m_out_speed`: 网速数据
- `m_cpu_usage`, `m_memory_usage`: CPU/内存利用率
- `m_cpu_temperature`, `m_gpu_temperature`: 温度数据
- `m_plugins`: 插件管理器

### 插件系统

插件接口定义在 `include/PluginInterface.h`：

- `ITMPlugin`: 插件主接口
- `IPluginItem`: 插件显示项目接口
- `ITrafficMonitor`: 主程序提供给插件的接口

开发插件需要：
1. 实现 `ITMPlugin` 和 `IPluginItem` 接口
2. 导出 `TMPluginGetInstance()` 函数
3. 将 DLL 放置在 `plugins` 目录下

### 主要模块

| 模块 | 文件 | 功能 |
|------|------|------|
| 主窗口 | TrafficMonitorDlg.cpp/h | 悬浮窗显示 |
| 任务栏窗口 | TaskBarDlg.cpp/h | 任务栏嵌入 |
| 皮肤系统 | SkinFile.cpp/h, SkinManager.cpp/h | 皮肤加载和管理 |
| 设置对话框 | OptionsDlg.cpp/h | 选项设置 |
| 历史流量 | HistoryTrafficFile.cpp/h | 流量统计 |
| 硬件监控 | OpenHardwareMonitorApi | 温度等硬件信息 |

## 开发约定

### 编码规范

- 使用 Unicode 字符集
- 字符串类型优先使用 `CString`、`wstring`
- 资源 ID 定义在 `resource.h`
- 使用预编译头 `stdafx.h`

### 预处理器宏

- `WITHOUT_TEMPERATURE`: 编译 Lite 版本时定义，禁用温度监控功能
- `COMPILE_FOR_WINXP`: 为 Windows XP 编译时定义
- `DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR`: 禁用 Windows11 小组件检测

### 定时器 ID

定义在 `stdafx.h`:
- `MAIN_TIMER (1234)`: 主定时器
- `TASKBAR_TIMER (1236)`: 任务栏定时器
- `MONITOR_TIMER (1238)`: 监控数据更新定时器

### 消息定义

- `MY_WM_NOTIFYICON`: 通知区图标消息
- `WM_TASKBAR_WND_CLOSED`: 任务栏窗口关闭
- `WM_MONITOR_INFO_UPDATED`: 监控信息更新
- `WM_SETTINGS_APPLIED`: 设置应用

## 依赖项

### 系统库
- pdh.lib: 性能数据助手
- Powrprof.lib: 电源管理
- Dwmapi.lib: 桌面窗口管理器

### 第三方库
- LibreHardwareMonitor: 硬件监控（仅标准版）
- GDI+: 图形绘制

### 运行时要求
- Microsoft Visual C++ 运行时
- Windows 7 及以上版本

## 皮肤系统

皮肤文件位于 `skins` 目录，每个皮肤一个子文件夹：
- `background.bmp/png`: 背景图片
- `background_l.bmp/png`: 大背景图片
- `skin.ini`: 皮肤配置（INI 格式）
- `skin.xml`: 皮肤配置（XML 格式，支持温度显示）

## 配置文件

- `config.ini`: 主配置文件
- `global_cfg.ini`: 全局配置（如便携模式）
- `history_traffic.dat`: 历史流量数据

## 相关链接

- [Wiki 文档](https://github.com/zhongyang219/TrafficMonitor/wiki)
- [皮肤制作教程](https://github.com/zhongyang219/TrafficMonitor/wiki/皮肤制作教程)
- [插件开发指南](https://github.com/zhongyang219/TrafficMonitor/wiki/插件开发指南)
