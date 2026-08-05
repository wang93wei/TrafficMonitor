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
