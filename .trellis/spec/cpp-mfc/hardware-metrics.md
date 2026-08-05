# Hardware Metrics

> Rules for PDH/DXGI metric collection, GPU memory, and fail-closed behavior.
> Hardware data is unreliable by default; the UI must never show a misleading
> number as if it were accurate.

---

## Where hardware data lives

`TrafficMonitor/PdhHardwareQuery/` collects CPU, CPU frequency, GPU usage, GPU
VRAM, and disk usage via PDH and DXGI:

| File | Provides |
|------|----------|
| `CPUUsage.cpp/.h`, `CpuFreq.cpp/.h` | CPU usage and frequency |
| `GpuUsage.cpp/.h` | GPU utilization |
| `GpuMemory.cpp/.h`, `GpuMemorySelection.h` | GPU VRAM total/used and adapter selection |
| `DiskUsage.cpp/.h` | Disk utilization |
| `PdhQuery.cpp/.h` | Non-copyable PDH query wrapper |

Full (non-Lite) builds additionally use `OpenHardwareMonitorApi/`
(`LibreHardwareMonitorLib.dll`) for temperature/sensor data via the
`OpenHardwareMonitor/` interface in `include/`. Lite builds compile with
`WITHOUT_TEMPERATURE` and omit all of this — see [Build & Config](./build-config.md).

## Fail closed for uncertain data

Metric helpers conventionally return `bool` plus sentinel values:

- Return `false` when the value is unavailable.
- Use `-1` (or a typed equivalent) as the "unavailable" sentinel for numeric
  metrics; the UI treats `-1` as "do not display".
- Clamp percentages to `[0, 100]`. Never render a value like `137%` CPU.
- **Do not display a misleading total** (e.g. summing VRAM across adapters, or
  showing GPU memory when no readable adapter was found). When in doubt, show
  nothing.

## GPU memory selection (covered by tests)

`TrafficMonitor/PdhHardwareQuery/GpuMemorySelection.h` is header-only selection
logic with explicit rules, and it is the only metric path covered by a
standalone regression test (`tests/gpu_memory_selection_test.cpp`).

`SelectPreferredAdapterMemoryLimit()` priority:

1. Prefer **discrete** adapters; fall back to **integrated**.
2. Skip software/unreadable adapters (`is_software == true`, or memory == 0).
3. Within an adapter kind, pick the largest readable memory limit.
4. PDH dedicated limit takes priority over DXGI dedicated/shared memory when both
   exist.
5. If **all** candidates are unknown/unreadable → fail closed (return `false`),
   and the UI shows no GPU memory.

When changing this logic, extend `tests/gpu_memory_selection_test.cpp` with the
new edge case (discrete preference, integrated fallback, unreadable/software
adapters, PDH priority, all-unreadable fail-closed). Run it standalone — see
[Quality & Testing](./quality-testing.md).

## Temperature / OpenHardwareMonitor stability

`LibreHardwareMonitorLib.dll` can be unstable on some hardware. The runtime
requires administrator rights for the standard/hardware-monitoring paths, and
hardware-monitor errors are surfaced via `WM_HARDWARE_MONITOR_ERROR`
(`stdafx.h:74`) — posted from the worker to the UI, never shown directly from the
worker thread (see [Monitoring & Threading](./monitoring-threading.md)).

Guard every temperature/sensor code path with `#ifndef WITHOUT_TEMPERATURE` so
Lite builds compile without the DLL.

## Plugin metric contract

Plugins receive monitor info via `ITMPlugin::OnMonitorInfo` /
`ITrafficMonitor::GetMonitorValue()` (`include/PluginInterface.h`). Built-in
metric values written into `theApp.m_*` are what the UI and plugins ultimately
read — keep them updated in `DoMonitorAcquisition()`.

Reference files:
- `TrafficMonitor/PdhHardwareQuery/GpuMemorySelection.h`
- `TrafficMonitor/PdhHardwareQuery/GpuMemory.cpp/.h`
- `tests/gpu_memory_selection_test.cpp`
- `include/PluginInterface.h`
