# Directory Structure

> Where different kinds of files live in TrafficMonitor.

This is a flat, single-binary C++/MFC project. There is no package manager,
`vcpkg`, or `CMakeLists.txt`. Files are organized by feature under a small
number of top-level directories.

---

## Top-level layout

| Path | Owns |
|------|------|
| `TrafficMonitor/` | The main application: app class, main dialog, drawing, settings, skins, taskbar integration, hardware helpers, resources. |
| `TrafficMonitor/PdhHardwareQuery/` | PDH/DXGI metric collection: CPU, CPU frequency, GPU usage, GPU VRAM, disk usage, and GPU memory selection logic. |
| `OpenHardwareMonitorApi/` | C++/CLI bridge to checked-in `LibreHardwareMonitorLib.dll` for temperature/sensor data. Only linked in full (non-Lite) builds. |
| `include/` | Public headers consumed across projects. `include/PluginInterface.h` is the plugin ABI; `include/OpenHardwareMonitor/` the hardware-monitor interface. |
| `PluginDemo/` | Reference plugin DLL implementation and export pattern. Copy this as the starting point for new plugins. |
| `tests/` | Standalone C++ assertion-style tests (no test framework). Currently `gpu_memory_selection_test.cpp` only. |
| `.github/workflows/` | Release CI (`main.yml`), pinned to `windows-2022`. |
| `TrafficMonitor/skins/`, `TrafficMonitor/language/`, `TrafficMonitor/res/` | Shipped skins, localization INIs, and Windows resources (.rc/.ico/.bmp). |
| `UpdateLog/`, `README*.md`, `Help*.md`, `IFLOW.md`, `皮肤制作教程.md` | User-facing docs and changelogs. |

Solutions:
- `TrafficMonitor.sln` — full build (main app + `OpenHardwareMonitorApi` + `PluginDemo`).
- `TrafficMonitor_Lite.sln` — Lite build used by CI; excludes the temperature project and defines `WITHOUT_TEMPERATURE`.

---

## Where new files go

| You are adding... | Put it in |
|-------------------|-----------|
| A new dialog / UI screen | `TrafficMonitor/`, paired `<Name>.cpp` + `<Name>.h` (e.g. `AboutDlg.cpp/.h`) |
| A new hardware metric / PDH counter | `TrafficMonitor/PdhHardwareQuery/` |
| A new display item (built-in, not plugin) | `TrafficMonitor/DisplayItem.cpp/.h` plus text mappings |
| A new taskbar embedding variant | `TrafficMonitor/`, deriving from `CTaskBarDlg` (`TrafficMonitor/TaskBarDlg.h`) |
| A new rendering backend | Implement `IDrawCommon` (`TrafficMonitor/IDrawCommon.h`) — do not fork drawing |
| A new plugin | New project under a `plugins/<name>/` folder, mirroring `PluginDemo/` |
| A pure-logic regression test | `tests/`, standalone `int main()` with `EXIT_FAILURE` on mismatch |

---

## Naming conventions

- Class files use PascalCase with the `C` prefix matching the class: class `CAboutDlg` → `AboutDlg.cpp` / `AboutDlg.h`. (Note: the file drops the `C`, the class keeps it.)
- Headers are paired one-to-one with their `.cpp` unless the header is interface-only or header-only (e.g. `GpuMemorySelection.h`).
- Resource IDs go in `TrafficMonitor/resource.h` (e.g. `IDD_ABOUT_DIALOG`, `IDS_*` for strings).
- Timer IDs and custom `WM_USER` messages are centralized in `TrafficMonitor/stdafx.h` — see [Monitoring & Threading](./monitoring-threading.md).

Reference files:
- `TrafficMonitor/stdafx.h` — central constants, messages, PCH.
- `TrafficMonitor/CommonData.h` — persisted settings and data structs.
- `TrafficMonitor/DisplayItem.h` — display-item model.
