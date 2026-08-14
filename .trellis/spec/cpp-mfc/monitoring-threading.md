# Monitoring & Threading

> The monitoring loop is the heart of the app. Getting the threading model wrong
> causes deadlocks, use-after-free, and hung UI. Follow these rules exactly.

---

## Architecture

Startup (`CTrafficMonitorDlg::OnInitDialog()`) starts three timers and a worker:

1. `MAIN_TIMER` (1234) — UI refresh.
2. `MONITOR_TIMER` (1238) — sets the `m_monitor_data_required` flag.
3. `MonitorThreadCallback()` — the worker thread.

Per cycle, `MONITOR_TIMER` flags that data is required; the worker calls
`DoMonitorAcquisition()`, writes results into `theApp.m_*` members, notifies
plugins, then **posts** `WM_MONITOR_INFO_UPDATED` so the UI thread can repaint
and update tooltips.

All timer IDs and custom messages are centralized in
`TrafficMonitor/stdafx.h`:

```text
TrafficMonitor/stdafx.h:69   MY_WM_NOTIFYICON          (WM_USER+1005)
TrafficMonitor/stdafx.h:70   WM_TASKBAR_WND_CLOSED     (WM_USER+1006)
TrafficMonitor/stdafx.h:71   WM_MONITOR_INFO_UPDATED   (WM_USER+1007)
TrafficMonitor/stdafx.h:72   WM_REOPEN_TASKBAR_WND     (WM_USER+1008)
TrafficMonitor/stdafx.h:73   WM_SETTINGS_APPLIED       (WM_USER+1009)
TrafficMonitor/stdafx.h:74   WM_HARDWARE_MONITOR_ERROR (WM_USER+1010)
TrafficMonitor/stdafx.h:75   WM_REINIT_CONNECTION      (WM_USER+1011)
TrafficMonitor/stdafx.h:84   MAIN_TIMER      1234
TrafficMonitor/stdafx.h:86   TASKBAR_TIMER   1236
TrafficMonitor/stdafx.h:88   MONITOR_TIMER   1238
```

Add any new timer ID or `WM_USER` message to `stdafx.h` — do not scatter magic
numbers through dialogs.

## Rule: never block or show modal UI from the worker thread

The worker must not call `MessageBox`, open a `CDialog`, or otherwise interact
with the UI directly. This is the single most important threading rule and the
cause of past deadlocks/use-after-free bugs.

**Correct pattern** (the one existing hardware-monitor fixes follow): on the
worker thread, store the message/error in a member, then `PostMessage` a UI
handler that runs on the UI thread and may show UI.

Reference:
- `TrafficMonitor/TrafficMonitorDlg.cpp:1625` — worker posts
  `PostMessage(WM_MONITOR_INFO_UPDATED)` after acquisition.
- `WM_HARDWARE_MONITOR_ERROR` (`stdafx.h:74`) — posted from the background thread
  to the UI; `wParam=0` means a normal error popup, `wParam=1` means an
  auto-disable notice.
- `WM_REINIT_CONNECTION` (`stdafx.h:75`) — posted from the monitoring worker when
  it detects adapter changes; the UI handler runs `IniConnection()`/`AutoSelect()`
  (`wParam=0`/`1`). These functions free/rebuild `m_pIfTable`, mutate
  `m_connections` and menus, so they must never be called directly from the
  worker thread.

## Rule: shared state must not be mutated from the worker thread

The worker acquires data and posts; it must not mutate containers, settings
strings, menus, or shared pointers that the UI thread reads:

- `m_pIfTable` / `m_connections` are guarded by `CTrafficMonitorDlg::m_iftable_critical`
  (`TrafficMonitorDlg.h`). Every cross-thread access — the worker's `GetIfTable`
  refresh and `GetConnectIfTable()` reads, the UI's `IniConnection()` rebuild —
  holds this lock (CRITICAL_SECTION is recursive; nesting is fine). Dialogs that
  outlive a re-init (e.g. `CNetworkInfoDlg`) receive a snapshot copy, never
  references/pointers into the shared table.
- Cross-thread settings strings (`m_cfg_data.m_connection_name`,
  `m_general_data.hard_disk_name`, `m_general_data.cpu_core_name`) are guarded by
  `m_settings_str_critical`. The worker only takes locked snapshots; when it
  needs to change a name (hardware fallback), it stores it via
  `SetPendingHardDiskName`/`SetPendingCpuCoreName` and the UI applies it in
  `OnMonitorInfoUpdated` → `ApplyPendingHwNames()`.
- `theApp.m_pMonitor` must be null-checked **inside** `m_minitor_lib_critical`
  (see `AcquireHardwareMonitorInfo`); `ApplySettings()` can `reset()` it under
  the same lock. Lock order: never hold `m_minitor_lib_critical` while acquiring
  `m_settings_str_critical`.
- Background helper threads (hardware-monitor init, update check) must release
  `m_minitor_lib_critical` before showing any `AfxMessageBox`, otherwise the
  monitoring worker stalls and exit-timeout teardown becomes a use-after-free.

## Global state is intentional

`extern CTrafficMonitorApp theApp` (`TrafficMonitor/TrafficMonitor.h`) owns config
paths, the current metric values (`m_in_speed`, `m_out_speed`, `m_cpu_usage`,
`m_gpu_memory_total`, …), settings structs, menus, plugins, and rendering
support. The worker writes `theApp.m_*`; the UI reads them on repaint. Treat
this as the design, not as tech debt to refactor away.

When adding a new metric: add the `theApp.m_<metric>` member, populate it in
`DoMonitorAcquisition()`, and read it in the relevant draw/tooltip handler on the
UI thread.

## Adding a new monitor timer / message

1. Define the ID in `TrafficMonitor/stdafx.h` (next free `WM_USER+n` / integer).
2. Add the handler to the message map of the owning dialog.
3. If it crosses threads, post (do not send) and pass ownership of any heap data.

Reference files:
- `TrafficMonitor/TrafficMonitorDlg.cpp`, `TrafficMonitor/TrafficMonitorDlg.h` —
  timers and the monitoring worker.
- `TrafficMonitor/stdafx.h` — all timer/message constants.
