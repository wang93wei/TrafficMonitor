# Repository Guidelines

## Project Overview

TrafficMonitor is a Windows desktop network-speed monitor built with C++20 and MFC. It shows live network speed, CPU/RAM usage, GPU/disk metrics, history traffic, skins, plugins, and taskbar-embedded widgets.

This repo is an independently maintained fork of `zhongyang219/TrafficMonitor`. Current fork-specific work centers on PDH/DXGI GPU VRAM reporting, GPU adapter selection, Win11 multi-monitor taskbar behavior, hardware-monitoring stability, and DPI/display-change handling.

## Architecture & Data Flow

- Startup: `CTrafficMonitorApp::InitInstance()` in `TrafficMonitor/TrafficMonitor.cpp` derives paths, loads language/config/plugin settings, enforces single-instance behavior, initializes plugins/GDI+/optional hardware monitoring, then runs `CTrafficMonitorDlg` modally.
- Global state is intentional: `extern CTrafficMonitorApp theApp` owns config paths, current metric values (`m_in_speed`, `m_cpu_usage`, `m_gpu_memory_total`, etc.), settings structs, menus, plugins, and rendering support.
- Monitoring loop: `CTrafficMonitorDlg::OnInitDialog()` starts `MAIN_TIMER`, `MONITOR_TIMER`, and `MonitorThreadCallback()`. `MONITOR_TIMER` sets `m_monitor_data_required`; the worker calls `DoMonitorAcquisition()`, writes `theApp.m_*`, notifies plugins, then posts `WM_MONITOR_INFO_UPDATED` for UI repaint/tooltips.
- Hardware data: PDH/DXGI helpers in `TrafficMonitor/PdhHardwareQuery/` provide CPU, CPU frequency, GPU usage, GPU VRAM, and disk usage. Standard builds also use `OpenHardwareMonitorApi/` for LibreHardwareMonitor temperature/sensor data; Lite builds compile with `WITHOUT_TEMPERATURE`.
- Rendering: main-window drawing uses `CSkinFile`; taskbar drawing uses `CTaskBarDlg` plus `ClassicalTaskbarDlg`, `Win11TaskbarDlg`, or `WineTaskbarDlg`. `IDrawCommon` abstracts GDI/D2D paths; D2D/DComposition support lives in `TaskBarDlgDrawCommon.*`.
- Plugins: `CPluginManager` loads `plugins/*.dll`, resolves `TMPluginGetInstance`, checks API version, passes `ITrafficMonitor*`, and folds `IPluginItem`s into `CommonDisplayItem` ordering/settings.
- Settings: persisted models are in `CommonData.h`; `CSettingsHelper`/`CIniHelper` read/write INI files. Options copy tab data into `theApp`, notify plugins, update/reopen taskbar windows when needed.

## Key Directories

- `TrafficMonitor/` — main MFC app, dialogs, drawing, settings, skins, resources, taskbar integration, crash/update/autostart helpers.
- `TrafficMonitor/PdhHardwareQuery/` — PDH/DXGI metric collection and GPU memory selection logic.
- `OpenHardwareMonitorApi/` — C++/CLI bridge to checked-in `LibreHardwareMonitorLib.dll` for standard hardware monitoring.
- `include/` — public plugin and hardware-monitoring interfaces; `include/PluginInterface.h` is the plugin ABI.
- `PluginDemo/` — sample plugin DLL implementation and export pattern.
- `tests/` — standalone C++ assertion-style tests; currently GPU memory selection only.
- `TrafficMonitor/skins/`, `TrafficMonitor/language/`, `TrafficMonitor/res/` — shipped skins, localization INIs, and Windows resources.
- `.github/workflows/` — release CI for Lite builds.
- `UpdateLog/`, `README*.md`, `Help*.md` — user-facing docs and changelogs.

## Spec-Driven Workflow (Trellis)

This repo uses a Trellis spec/task workflow. Before non-trivial edits, check for context here:

- `.trellis/spec/cpp-mfc/` — authoritative per-topic specs that go deeper than this file: `build-config.md`, `coding-conventions.md`, `directory-structure.md`, `monitoring-threading.md`, `hardware-metrics.md`, `drawing-taskbar.md`, `plugins.md`, `settings-strings.md`, `quality-testing.md`. Read the relevant spec before changing sensitive areas (monitoring thread, hardware/PDH/DXGI, taskbar drawing, plugins, settings).
- `.trellis/spec/guides/` — thinking guides (`code-reuse-thinking-guide.md`, `cross-layer-thinking-guide.md`).
- `.trellis/tasks/` — current task files; `.trellis/tasks/archive/` — completed. `.trellis/spec/index.md` and `spec/cpp-mfc/index.md` are the entry points.
- Skills `trellis-before-dev`, `trellis-check`, `trellis-update-spec`, etc. (under `.agents/skills/`) and the `/trellis` command drive the workflow. When a change touches a spec'd area, update the corresponding `.trellis/spec/` doc alongside the code.

## Development Commands

Run from a VS 2022 Developer Command Prompt unless noted.

```bat
:: CI-equivalent Lite release builds
msbuild TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=x64 -p:platformToolset=v143
msbuild TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=x86 -p:platformToolset=v143
msbuild TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=ARM64EC -p:platformToolset=v143

:: Full standard build, includes OpenHardwareMonitorApi
msbuild TrafficMonitor.sln -p:configuration=release -p:platform=x64 -p:platformToolset=v143
```

Git Bash example with Build Tools MSBuild:

```bash
"/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe" TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=x64 -p:platformToolset=v143
```

Notes:
- Main project pre-build runs `TrafficMonitor/print_compile_time.bat`, which rewrites `TrafficMonitor/compile_time.txt`. If a bash build cannot find the batch file, run from `cmd`/Developer Prompt or pass `-p:PreBuildEventUseInBuild=false` when appropriate.
- Outputs: x86 uses `Bin/<Configuration>/`; x64 and ARM64EC use `Bin/<Platform>/<Configuration>/`. Plugin DLLs output under the matching `plugins/` folder.
- There is no repo-level lint or formatter command observed.

## Code Conventions & Common Patterns

- C++20, Unicode, dynamic MFC, PCH via `TrafficMonitor/stdafx.h`. Use existing MFC idioms: `C*` classes, `m_` members, `CString`, `wstring`, `_T()`, `BOOL`/`TRUE`/`FALSE`, `BEGIN_MESSAGE_MAP`, `afx_msg`, `ASSERT`.
- Respect compile guards: `WITHOUT_TEMPERATURE`, `COMPILE_FOR_WINXP`, `DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR`, `_DEBUG`, `_M_ARM64EC`, `_M_X64`.
- Do not introduce a second settings, plugin, display-item, or drawing abstraction beside `CSettingsHelper`/`CIniHelper`, `CPluginManager`, `CommonDisplayItem`, and `IDrawCommon`.
- User-visible strings should go through `CCommon::LoadText`/`CStrTable` and `TrafficMonitor/language/*.ini`; resource IDs belong in `TrafficMonitor/resource.h`.
- Worker-thread code must not show modal UI directly. Existing hardware-monitor fixes store messages and `PostMessage` UI handlers; preserve that pattern to avoid deadlocks/use-after-free.
- Metric helpers usually return `bool` plus sentinel values (`-1` unavailable) and clamp percentages to `[0, 100]`. Fail closed for uncertain hardware data; do not display misleading totals.
- Resource ownership is mixed legacy/new code: raw Win32/MFC handles, `malloc/free`, `new/delete`, plus `ComPtr`, `shared_ptr`, locks. Preserve non-copyable resource wrappers such as `CPdhQuery` and `CSkinFile`.
- Plugin item values are queried frequently. Follow `include/PluginInterface.h`: collect data in `ITMPlugin::DataRequired()`, then return cached text from `IPluginItem::GetItemValueText()`.

## Important Files

- `TrafficMonitor.sln` — full solution: main app, `OpenHardwareMonitorApi`, `PluginDemo`.
- `TrafficMonitor_Lite.sln` — Lite solution used by CI; excludes temperature project.
- `TrafficMonitor/TrafficMonitor.vcxproj` — main app build settings, link dependencies, lite/full defines.
- `TrafficMonitor/TrafficMonitor.cpp`, `TrafficMonitor/TrafficMonitor.h` — app lifecycle and global state/service surface.
- `TrafficMonitor/TrafficMonitorDlg.cpp`, `TrafficMonitor/TrafficMonitorDlg.h` — main dialog, timers, monitoring worker, taskbar coordination.
- `TrafficMonitor/stdafx.h` — timers, custom `WM_USER` messages, app constants, version macros.
- `TrafficMonitor/CommonData.h` — persisted settings and data structs.
- `TrafficMonitor/DisplayItem.h`, `TrafficMonitor/DisplayItem.cpp` — built-in/plugin display item model and UI text mapping.
- `TrafficMonitor/PdhHardwareQuery/GpuMemorySelection.h` — header-only GPU adapter/VRAM selection logic covered by tests.
- `TrafficMonitor/TaskBarDlg.*`, `TrafficMonitor/Win11TaskbarDlg.*`, `TrafficMonitor/ClassicalTaskbarDlg.*`, `TrafficMonitor/WineTaskbarDlg.*` — taskbar embedding variants.
- `include/PluginInterface.h` — plugin ABI, API version 7, host/plugin interfaces.
- `.github/workflows/main.yml` — release CI pinned to `windows-2022`.
- `tests/gpu_memory_selection_test.cpp` — standalone regression test for GPU memory selection.
- `version_utf8.info` — readable release/update metadata; prefer it over legacy `version.info` when editing UTF-8 content.

## Runtime/Tooling Preferences

- Required toolchain: Visual Studio 2022, MSVC v143, MFC, Windows SDK 10.0, Unicode, C++20.
- CI intentionally uses `windows-2022`; comments say newer `windows-latest` images lacked MFC/v143 for this project.
- Supported platforms: x86 (`Win32` in project files), x64, ARM64EC.
- Full standard builds link `OpenHardwareMonitorApi.lib`, require C++/CLI/.NET Framework 4.7.2 for the hardware DLL, and request administrator rights. Lite builds define `WITHOUT_TEMPERATURE`, omit OpenHardwareMonitorApi linkage, and do not require admin.
- Runtime requires Microsoft Visual C++ redistributable. Standard/hardware-monitoring paths depend on `LibreHardwareMonitorLib.dll` and can be unstable on some hardware.
- Runtime files are expected beside `TrafficMonitor.exe` in portable mode: `config.ini`, `global_cfg.ini`, `history_traffic.dat`, `skins/`, `plugins/`, `language/`; non-writable installs fall back to `%APPDATA%\TrafficMonitor`.
- No `CMakeLists.txt`, `vcpkg.json`, package manager lockfile, or Node/Bun tooling was observed. Do not add new tooling unless the task explicitly needs it.

## Testing & QA

- There is no integrated test project or CI test step. CI only builds Lite release artifacts.
- Standalone pure test:

```bat
cl /EHsc /std:c++20 /I. tests\gpu_memory_selection_test.cpp
gpu_memory_selection_test.exe
```

- Extend `tests/gpu_memory_selection_test.cpp` for `GpuMemorySelection::SelectPreferredAdapterMemoryLimit` edge cases: discrete preference, integrated fallback, unreadable/software adapters, PDH limit priority, unknown/all-unreadable fail-closed.
- `_DEBUG` app startup calls `CTest::Test()` from `TrafficMonitor/Test.cpp`; this is ad hoc/debug-only and not a substitute for regression tests.
- For UI/taskbar/plugin/hardware changes, build the narrowest relevant config and perform manual smoke checks on Windows. Cover taskbar window open/reopen, DPI/display changes, language strings, plugin loading, and hardware unavailable/error paths.
