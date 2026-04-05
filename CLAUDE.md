# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

TrafficMonitor is a Windows desktop network speed monitoring utility built with C++/MFC. It displays real-time network speed, CPU/memory utilization, hardware temperatures, and supports taskbar embedding, skinning, and plugins.

## Build System

- **Toolchain**: Visual Studio 2022 (v17+), MSVC v143 toolset
- **Solution**: `TrafficMonitor.sln` containing 3 projects:
  - `TrafficMonitor` — main application (MFC dialog-based)
  - `OpenHardwareMonitorApi` — hardware monitoring library wrapping LibreHardwareMonitor
  - `PluginDemo` — sample plugin DLL

### Build Configurations

| Configuration | Description | Preprocessor Define |
|---|---|---|
| `Debug` / `Release` | Full standard version | (none) |
| `Debug (lite)` / `Release (lite)` | No hardware monitoring | `WITHOUT_TEMPERATURE` |

### Build Commands (MSBuild)

```bash
# Standard x64 release
msbuild -p:configuration=release -p:platform=x64 -p:platformToolset=v143

# Lite x64 release (no hardware monitoring)
msbuild -p:configuration="release (lite)" -p:platform=x64 -p:platformToolset=v143

# Standard x86 debug
msbuild -p:configuration=debug -p:platform=x86 -p:platformToolset=v143

# ARM64EC
msbuild -p:configuration=release -p:platform=ARM64EC -p:platformToolset=v143
```

Build output goes to `Bin/{platform}/{configuration}/`.

### CI

GitHub Actions (`.github/workflows/main.yml`) builds x64, x86, and ARM64EC release configurations on push.

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
- Plugin interface: `ITMPlugin` (plugin) + `IPluginItem` (display items)
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
- Config file: `config.ini` in the executable directory or AppData

### Display Items

Built-in display items defined in `DisplayItem.h` enum `DisplayItem`: `TDI_UP`, `TDI_DOWN`, `TDI_CPU`, `TDI_MEMORY`, `TDI_GPU_USAGE`, temperature items (gated by `WITHOUT_TEMPERATURE`), `TDI_HDD_USAGE`, `TDI_CPU_FREQ`, `TDI_TOTAL_SPEED`, `TDI_TODAY_TRAFFIC`.

### Localization

- Language files in `TrafficMonitor/language/` as INI files (e.g., `Simplified_Chinese.ini`, `English.ini`)
- `language.h` / `StrTable.h` — string table management

### Data Structures

- `CommonData.h` — global structs and enums (Date, HistoryTraffic, SpeedUnit, HardwareItem, DispStrings)
- `Common.h/cpp` — shared utility functions
- `CVariant` — variant type wrapper

## Key Patterns

- `WITHOUT_TEMPERATURE` preprocessor guard controls hardware monitoring feature inclusion across many files
- Custom Windows messages (WM_USER+10xx) for inter-component communication (defined in stdafx.h)
- MFC dialog-based architecture with multiple modeless dialogs (main window, taskbar window, options)
- Timer-driven polling: `MAIN_TIMER`, `TASKBAR_TIMER`, `MONITOR_TIMER` for periodic data updates
