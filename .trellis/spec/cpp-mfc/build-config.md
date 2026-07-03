# Build & Config

> Solutions, configurations, compile guards, CI, and output directories.

---

## Toolchain

- Visual Studio 2022, MSVC **v143**, MFC (dynamic), Windows SDK 10.0, Unicode, C++20.
- There is no `CMakeLists.txt`, `vcpkg.json`, or package-manager lockfile. Do not
  add new tooling unless a task explicitly requires it.

## Solutions & configurations

| Solution | Includes | Notes |
|----------|----------|-------|
| `TrafficMonitor.sln` | Main app + `OpenHardwareMonitorApi` + `PluginDemo` | Full build; links `OpenHardwareMonitorApi.lib`; requires C++/CLI / .NET Framework 4.7.2; requests admin rights. |
| `TrafficMonitor_Lite.sln` | Main app only | CI build; defines `WITHOUT_TEMPERATURE`; no OpenHardwareMonitorApi linkage; no admin required. |

Configurations: `Debug`, `Release`, `Debug (lite)`, `Release (lite)`. Platforms:
x86 (`Win32`), x64, ARM64EC.

## Compile guards

| Guard | Effect |
|-------|--------|
| `WITHOUT_TEMPERATURE` | Lite builds — excludes temperature/hardware-monitor code and `OpenHardwareMonitorApi` linkage. Set via `<PreprocessorDefinitions>` in `TrafficMonitor/TrafficMonitor.vcxproj`. |
| `COMPILE_FOR_WINXP` | Legacy WinXP target (mostly dormant). |
| `DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR` | Disables the Win11 widget detector. |
| `_DEBUG` | Debug builds; also triggers ad-hoc `CTest::Test()` at startup (`TrafficMonitor/Test.cpp`) — not a test suite. |
| `_M_ARM64EC`, `_M_X64` | Platform-specific paths. |

Every temperature/sensor code path must be `#ifndef WITHOUT_TEMPERATURE`-guarded
so Lite builds compile without `LibreHardwareMonitorLib.dll`.

## Build commands

Run from a VS 2022 Developer Command Prompt (or Git Bash with the full MSBuild path):

```bat
:: CI-equivalent Lite builds
msbuild TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=x64 -p:platformToolset=v143
msbuild TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=x86 -p:platformToolset=v143
msbuild TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=ARM64EC -p:platformToolset=v143

:: Full standard build
msbuild TrafficMonitor.sln -p:configuration=release -p:platform=x64 -p:platformToolset=v143
```

Git Bash with Build Tools MSBuild:

```bash
"/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe" TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=x64 -p:platformToolset=v143
```

### Pre-build note

The main project's pre-build step runs `TrafficMonitor/print_compile_time.bat`,
which rewrites `TrafficMonitor/compile_time.txt`. If a bash build cannot find the
batch file, run from `cmd`/Developer Prompt, or pass
`-p:PreBuildEventUseInBuild=false` when appropriate.

## Output directories

| Platform | Output |
|----------|--------|
| x86 | `Bin/<Configuration>/` |
| x64 | `Bin/x64/<Configuration>/` |
| ARM64EC | `Bin/ARM64EC/<Configuration>/` |

Plugin DLLs output under the matching `plugins/` folder.

## CI (`.github/workflows/main.yml`)

Release CI builds Lite artifacts for x64, x86, and ARM64EC, triggered on `v*`
tags or manual dispatch.

- **Runner is pinned to `windows-2022`** — intentionally. Comments in
  `main.yml:9-12` and `main.yml:45,80` note that `windows-latest` migrated to a
  VS2026 image (2026-06) that lacks the MFC libraries (MSB8041) and the v143
  toolset (MSB8020). Do not "modernize" this to `windows-latest` without
  confirming MFC/v143 availability.
- Uses `actions/checkout@v6`, `microsoft/setup-msbuild@v3`, uploads `.exe`/`.dll`
  and `.pdb` artifacts.

## Version metadata

`version_utf8.info` is the readable release/update metadata; prefer it over the
legacy `version.info` when editing UTF-8 content. The version macro is
`VERSION` in `TrafficMonitor/stdafx.h`.

Reference files:
- `TrafficMonitor.sln`, `TrafficMonitor_Lite.sln`
- `TrafficMonitor/TrafficMonitor.vcxproj`
- `.github/workflows/main.yml`
- `version_utf8.info`, `version.info`, `TrafficMonitor/compile_time.txt`
