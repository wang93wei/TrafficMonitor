# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

TrafficMonitor is a Windows desktop network speed monitoring utility built with C++/MFC. It displays real-time network speed, CPU/memory utilization, hardware temperatures, and supports taskbar embedding, skinning, and plugins.

## Repository Context

This repo is an independently-maintained fork of [zhongyang219/TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor), developed on the `backup-master` branch (upstream `master` is the default PR target). Active work focuses on:

- **GPU VRAM monitoring** — PDH/DXGI-based dedicated VRAM usage (`TDI_GPU_MEMORY`); auto-hides when total VRAM can't be reliably determined. See `TrafficMonitor/PdhHardwareQuery/GpuMemorySelection.h` (header-only) and `tests/gpu_memory_selection_test.cpp`.
- **GPU adapter selection** — prefer discrete GPUs; align PDH adapter with DXGI total-VRAM; fall back gracefully.
- **Win11 multi-monitor taskbar** — fixes for secondary-display positioning, layout stabilization, VRAM item spacing.
- **Hardware monitor stability** — error-handling fixes to prevent infinite dialogs/hangs.
- **DPI / display-change handling** — option-tab DPI consistency and taskbar window rebuild on resolution change.

Sibling agent instruction files: `AGENTS.md` is the source-of-truth (this file); `CLAUDE.md` is a symlink to it; `IFLOW.md` is a Chinese-language context doc with additional dependency/version details (current declared version: 1.85.1, C++20, Unicode charset, Windows SDK 10).

Upstream has announced temperature monitoring is being phased out of the main app (moved to a plugin), and future releases will ship only the Lite build.

## Build System

- **Toolchain**: Visual Studio 2022 (v17+), MSVC v143 toolset
- **Solutions**:
  - `TrafficMonitor.sln` — full solution with 3 projects: `TrafficMonitor` (main app), `OpenHardwareMonitorApi` (hardware monitoring), `PluginDemo` (sample plugin)
  - `TrafficMonitor_Lite.sln` — lite solution with 2 projects: `TrafficMonitor`, `PluginDemo` (no OpenHardwareMonitorApi)

### Build Configurations

| Configuration | Description | Preprocessor Define |
|---|---|---|
| `Debug` / `Release` | Full standard version | (none) |
| `Debug (lite)` / `Release (lite)` | No hardware monitoring | `WITHOUT_TEMPERATURE` |

### Build Commands (MSBuild)

**MSBuild location**: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe`

In bash/Git Bash, use the quoted full path. In cmd/Developer Command Prompt, `msbuild` is on PATH.

```bash
# In bash/Git Bash — use full path with quotes
"/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe" TrafficMonitor_Lite.sln -p:configuration="release (lite)" -p:platform=x64 -p:platformToolset=v143

# In Developer Command Prompt / cmd — msbuild is on PATH
msbuild TrafficMonitor.sln -p:configuration=release -p:platform=x64 -p:platformToolset=v143
msbuild TrafficMonitor_Lite.sln -p:configuration="release (lite)" -p:platform=x64 -p:platformToolset=v143

# ARM64EC lite release
msbuild TrafficMonitor_Lite.sln -p:configuration="release (lite)" -p:platform=ARM64EC -p:platformToolset=v143
```

Supported platforms: x86, x64, ARM64EC. Build output goes to `Bin/{platform}/{configuration}/`.

### Build Notes

- **PreBuildEvent issue**: `TrafficMonitor.vcxproj` has a PreBuildEvent that runs `print_compile_time.bat`. When building from bash (not cmd), the batch file may not be found, causing `MSB3073` error. Workarounds:
  1. Run the batch manually first: `cmd.exe //c "cd TrafficMonitor && .\print_compile_time.bat"`
  2. Skip the PreBuildEvent: add `-p:PreBuildEventUseInBuild=false` to the msbuild command
- `print_compile_time.bat` writes the current date/time to `TrafficMonitor/compile_time.txt`.

### CI

GitHub Actions (`.github/workflows/main.yml`) builds Lite release for x64, x86, and ARM64EC. Triggers on tag push (`v*`) and manual workflow dispatch.

### Running

Build output lands in `Bin/{platform}/{configuration}/TrafficMonitor.exe` (x86 omits the platform dir — `Bin/Release/`). Standard builds require elevation; Lite builds do not. The exe looks for `config.ini`, `global_cfg.ini`, `skins/`, `plugins/`, and `language/` next to itself (or under `%APPDATA%\TrafficMonitor` in non-portable mode). To debug, open the solution in Visual Studio 2022 and F5 from the `TrafficMonitor` project with the desired configuration/platform.

### Testing

There is no full test framework. `tests/gpu_memory_selection_test.cpp` is a standalone console `main()` with assertions over `GpuMemorySelection::SelectPreferredAdapterMemoryLimit`. Build it from a Developer Command Prompt:

```bash
cl /EHsc /std:c++20 /I. tests/gpu_memory_selection_test.cpp
```

Run the resulting `.exe` — exit code 0 means all assertions passed. Add new scenarios by extending the `ExpectPreferredLimit` / `ExpectNoLimit` calls in `main()`.

## Architecture

### Entry Point & Main Window

- `CTrafficMonitorApp` (TrafficMonitor.h/cpp) — CWinApp subclass, handles initialization, crash reporting, config loading
- `CTrafficMonitorDlg` (TrafficMonitorDlg.h/cpp) — main dialog, owns timers, coordinates all subsystems
- Singleton access via `CTrafficMonitorDlg::Instance()`

### Hardware Monitoring (Dual Backend)

1. **PDH (Performance Data Helper)** — `TrafficMonitor/PdhHardwareQuery/` — CPU usage, CPU frequency, GPU usage, disk usage via Windows PDH API
2. **LibreHardwareMonitor** — `OpenHardwareMonitorApi/` — wraps LibreHardwareMonitorLib.dll for temperature sensors, GPU utilization, advanced hardware metrics

Standard builds use both backends. Lite builds (`WITHOUT_TEMPERATURE`) exclude LibreHardwareMonitor entirely.

### Display & Rendering Pipeline

- `IDrawCommon` / `CDrawCommon` — GDI-based core drawing interface
- `CDrawCommonEx` — GDI+ extension layer for alpha blending and effects
- `CD2D1Support` — Direct2D hardware acceleration (optional)
- `DrawCommonFactory` — factory for creating rendering backend instances
- `CTaskBarDlg` / `TaskBarDlgDrawCommon` — taskbar window with its own drawing pipeline

### Taskbar Integration

- `CTaskBarDlg` — embeds into Windows taskbar, supports Win11/Classic/Wine taskbar styles
- `CTaskbarHelper` — multi-monitor taskbar detection and secondary display enumeration
- Specialized dialogs: `Win11TaskbarDlg`, `ClassicalTaskbarDlg`, `WineTaskbarDlg`

### Plugin System

- `CPluginManager` — loads DLL plugins from `plugins/` directory at runtime
- Plugin interface: `ITMPlugin` (plugin, API version 7) + `IPluginItem` (display items)
- `ITrafficMonitor` — host program interface exposed to plugins
- Plugin display items integrate alongside built-in `DisplayItem` enum items
- See `PluginDemo/` for a sample plugin implementation

### Skinning

- `CSkinManager` — skin loading, caching, and selection
- `CSkinFile` — parses XML-based skin configs (`skin.xml`) and legacy INI skins (`skin.ini`)
- Skins stored in `TrafficMonitor/skins/`, each in its own folder with background images and config
- Supports BMP/PNG backgrounds with customizable fonts, colors, and layout

### Configuration & Settings

- `CIniHelper` — INI file reader/writer with UTF-8 BOM support
- `CSettingsHelper` — app-specific settings persistence (fonts, colors, window positions)
- Config files (next to exe, or `%APPDATA%\TrafficMonitor` in non-portable mode):
  - `config.ini` — main user settings
  - `global_cfg.ini` — cross-instance globals (e.g., portable-mode flag)
  - `history_traffic.dat` — historical traffic statistics

### Display Items

Built-in display items defined in `DisplayItem.h` enum `DisplayItem`: `TDI_UP`, `TDI_DOWN`, `TDI_CPU`, `TDI_MEMORY`, `TDI_GPU_USAGE`, temperature items (gated by `WITHOUT_TEMPERATURE`), `TDI_HDD_USAGE`, `TDI_CPU_FREQ`, `TDI_TOTAL_SPEED`, `TDI_TODAY_TRAFFIC`, `TDI_GPU_MEMORY` (VRAM usage via PDH).

### Localization

- Language files in `TrafficMonitor/language/` as INI files (e.g., `Simplified_Chinese.ini`, `English.ini`)
- `language.h` / `StrTable.h` — string table management

### Data Structures

- `CommonData.h` — global structs and enums (Date, HistoryTraffic, SpeedUnit, HardwareItem, DispStrings)
- `Common.h/cpp` — shared utility functions
- `CVariant` — variant type wrapper

## Key Patterns

### Preprocessor Guards

- `WITHOUT_TEMPERATURE` — defined for Lite builds; gates hardware monitoring (LibreHardwareMonitor, temperature items) across many files
- `COMPILE_FOR_WINXP` — legacy Windows XP build target
- `DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR` — disables Win11 "Windows Web Experience Pack" presence detection (used by the Win11 taskbar path)

### Link Dependencies

System libs: `pdh.lib` (PDH hardware query), `Powrprof.lib` (CPU frequency), `Dwmapi.lib` (composition). Third-party: LibreHardwareMonitorLib.dll (standard build only, via `OpenHardwareMonitorApi`), GDI+.

### Inter-Component Messages

Custom Windows messages defined in `stdafx.h`:

- `MY_WM_NOTIFYICON` (WM_USER+1005) — notification icon events
- `WM_TASKBAR_WND_CLOSED` (WM_USER+1006) — taskbar window closed
- `WM_MONITOR_INFO_UPDATED` (WM_USER+1007) — monitor info updated
- `WM_REOPEN_TASKBAR_WND` (WM_USER+1008) — reopen taskbar window
- `WM_SETTINGS_APPLIED` (WM_USER+1009) — settings applied from options dialog
- `WM_NEXT_USER_MSG` (WM_USER+1011) — next available custom message slot

### Runtime Architecture

- MFC dialog-based with multiple modeless dialogs (main window, taskbar window, options)
- Timer-driven polling: `MAIN_TIMER` (1234), `DELAY_TIMER` (1235), `TASKBAR_TIMER` (1236), `CONNECTION_DETAIL_TIMER` (1237), `MONITOR_TIMER` (1238), `DELETE_NOTIFY_ICON_TIMER` (1239), `RESTART_TASKBAR_TIMER` (1240), `INIT_CONNECT_TIMER` (1241), `DPI_CHANGE_TIMER` (1242)
