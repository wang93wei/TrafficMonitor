# Coding Conventions

> How C++/MFC code is written in this repository. Match the surrounding code;
> this file documents what that actually means here.

---

## Language & toolchain baseline

- **C++20**, Unicode charset, dynamic MFC, MSVC v143 (Visual Studio 2022), Windows SDK 10.0.
- Precompiled header is `TrafficMonitor/stdafx.h`; new `.cpp` files in the main
  project typically `#include "stdafx.h"` first.
- Platforms: x86 (`Win32` in project files), x64, ARM64EC.

## Idioms

- MFC class naming: `C*` classes, `m_` member prefix, `afx_msg`, `BEGIN_MESSAGE_MAP`.
- String types: `CString`, `std::wstring`, `_T(...)` for literals, `L"..."` for
  wide literals. Prefer `CString` at MFC boundaries.
- Booleans at Win32/MFC boundaries use `BOOL`/`TRUE`/`FALSE`; plain `bool` is fine
  in internal logic.
- Assertions: `ASSERT(...)` (MFC). Use it for invariant checks, not for
  recoverable runtime errors.

## Resource ownership (mixed legacy + modern — preserve it)

The codebase deliberately mixes ownership styles. Do not "normalize" a file to a
single style as part of an unrelated change.

- Raw Win32/MFC handles (`HICON`, `HDC`, `HMODULE`) — released with the matching
  Win32 call.
- `malloc/free` and `new/delete` — both appear in legacy code.
- `ComPtr<T>` (WRL) for COM/DXGI/D2D resources — the preferred modern form for new
  COM work.
- `std::shared_ptr` / `std::unique_ptr` where lifecycle is non-trivial.
- `CLazyConstructable<T>` (`TrafficMonitor/TrafficMonitor.h`) wraps dependencies
  that initialize on first use (e.g. D2D taskbar draw support).
- Non-copyable resource wrappers must stay non-copyable. Examples:
  `CPdhQuery` (`TrafficMonitor/PdhHardwareQuery/PdhQuery.h`), `CSkinFile`.

### `SAFE_DELETE` macro

`TrafficMonitor/stdafx.h:116` defines the project-wide delete-and-null macro:

```cpp
#define SAFE_DELETE(p) do \
{\
    if(p != nullptr) \
    { \
        delete p; \
        p = nullptr; \
    } \
} while (false)
```

Use `SAFE_DELETE(p)` for legacy `new`-owned pointers where the surrounding code
already uses it. For new code prefer `std::unique_ptr`/`std::shared_ptr` over
manual `SAFE_DELETE`.

## Required abstractions (do not create alternatives)

The project intentionally has one of each of these. Do not introduce a second:

| Concern | Single source of truth |
|---------|------------------------|
| Settings read/write | `CSettingsHelper` / `CIniHelper` (`TrafficMonitor/SettingsHelper.h`, `TrafficMonitor/IniHelper.h`) |
| Plugin management | `CPluginManager` (`TrafficMonitor/PluginManager.h`) |
| Display items | `CommonDisplayItem` (`TrafficMonitor/DisplayItem.h`) |
| Drawing | `IDrawCommon` (`TrafficMonitor/IDrawCommon.h`) and its impls |

See [Settings & Strings](./settings-strings.md), [Plugins](./plugins.md),
[Drawing & Taskbar](./drawing-taskbar.md).

## Compile guards to respect

- `WITHOUT_TEMPERATURE` — Lite builds; disables `OpenHardwareMonitorApi` linkage and temperature code paths.
- `COMPILE_FOR_WINXP` — legacy WinXP target (mostly dormant).
- `DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR` — disables the Win11 widget detector.
- `_DEBUG` — debug-only code; `_DEBUG` startup runs `CTest::Test()` (`TrafficMonitor/Test.cpp`), which is ad hoc and not a test suite.
- `_M_ARM64EC`, `_M_X64` — platform-specific code paths.

Guard hardware/temperature code so Lite builds compile without it. See
[Build & Config](./build-config.md).

## User-visible text

User-visible strings must go through `CCommon::LoadText` / `CStrTable`
(`TrafficMonitor/Common.h:154`) and `TrafficMonitor/language/*.ini`, with resource
IDs in `TrafficMonitor/resource.h`. Do not hardcode user-facing literals. See
[Settings & Strings](./settings-strings.md).

## Anti-patterns to avoid

- **Modal UI from a worker thread.** Worker-thread code must not show dialogs
  directly — store a message and `PostMessage` a UI handler. See
  [Monitoring & Threading](./monitoring-threading.md).
- **Adding a parallel settings/drawing/plugin abstraction.** Extend the existing one.
- **Displaying uncertain hardware data as if it were accurate.** Fail closed. See
  [Hardware Metrics](./hardware-metrics.md).
- **"Normalizing" resource ownership** in a file you touched for an unrelated reason.
- **Hardcoding English/UI strings** instead of routing through `LoadText`.
