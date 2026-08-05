# C++/MFC Development Guidelines

> Conventions for working in the TrafficMonitor C++20/MFC Windows desktop codebase.

TrafficMonitor is a single-binary desktop app (no frontend/backend split). All
source-level guidance lives in this layer. Cross-cutting thinking guides live
in [`../guides/`](../guides/index.md).

---

## Guidelines Index

| Guide | When to read |
|-------|--------------|
| [Directory Structure](./directory-structure.md) | Adding files, deciding where a new module belongs |
| [Coding Conventions](./coding-conventions.md) | Writing or touching C++/MFC code of any kind |
| [Monitoring & Threading](./monitoring-threading.md) | Anything that touches timers, the worker thread, or metrics |
| [Hardware Metrics](./hardware-metrics.md) | PDH/DXGI data, GPU memory, CPU/disk usage, fail-closed rules |
| [Drawing & Taskbar](./drawing-taskbar.md) | Main-window drawing, skin rendering, taskbar embedding, D2D |
| [Settings & Strings](./settings-strings.md) | Persisted settings, INI files, localized user-visible text |
| [Plugins](./plugins.md) | Plugin ABI, loading, plugin display items |
| [Build & Config](./build-config.md) | Solutions, configurations, compile guards, CI, output dirs |
| [Quality & Testing](./quality-testing.md) | Before marking work done; build/test strategy and smoke checks |

---

## TL;DR for any change

1. C++20, Unicode, dynamic MFC, PCH via `TrafficMonitor/stdafx.h`. Match existing idioms.
2. Never block the UI thread or show modal UI from a worker thread — `PostMessage` instead.
3. Fail closed for uncertain hardware data; never display misleading totals.
4. Do not add a second settings/plugin/display-item/drawing abstraction — the existing ones are mandatory.
5. Build the narrowest relevant config and smoke-test on Windows before claiming done.

**Language**: all documentation in this directory is written in **English**.
