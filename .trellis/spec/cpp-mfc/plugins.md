# Plugins

> The plugin ABI, loading flow, and the display-item caching contract.
> `include/PluginInterface.h` is the contract — changing it is a versioned, breaking act.

---

## ABI: `include/PluginInterface.h`

Current **API version is 7** (`ITMPlugin::GetAPIVersion()`, `PluginInterface.h:166`).
The host checks the version on load. Three interfaces:

| Interface | Role |
|-----------|------|
| `ITMPlugin` | Plugin main interface. The host calls `DataRequired()` periodically and `OnInitialize(ITrafficMonitor*)` on load. |
| `IPluginItem` | One display item. A plugin may expose several via `ITMPlugin::GetItem(index)`. |
| `ITrafficMonitor` | Host surface offered back to plugins (`GetMonitorValue`, `GetStringRes`, `GetDPI`, `GetPluginConfigDir`, …). |

Plugin DLLs must export:

```cpp
ITMPlugin* TMPluginGetInstance();
```

The returned object should be global/static and live until process exit
(documented at `PluginInterface.h:426-431`).

## The caching contract (critical)

`IPluginItem` methods are **called frequently** (every paint). The contract is
documented inline at `PluginInterface.h:30-36`:

- Collect/acquire monitoring data **only** in `ITMPlugin::DataRequired()`.
- `IPluginItem::GetItemValueText()` must **return cached text** — never do data
  acquisition there.

A plugin that fetches data inside `GetItemValueText()` will stall the UI. New
plugin code and host-side plugin handling must preserve this split.

## Loading flow (`CPluginManager`)

`TrafficMonitor/PluginManager.cpp` / `.h`:

1. Loads each `plugins/*.dll` via `LoadLibrary`.
2. Resolves `TMPluginGetInstance` (`PluginManager.cpp:70-71`; typedef
   `pfTMPluginGetInstance` at `PluginManager.h:7`).
3. Calls it to get the `ITMPlugin*` (`PluginManager.cpp:78`).
4. Checks `GetAPIVersion()` against the supported version.
5. Passes the `ITrafficMonitor*` host interface to the plugin.
6. Folds each plugin's `IPluginItem`s into the shared `CommonDisplayItem`
   ordering/settings (`TrafficMonitor/DisplayItem.h`).

Plugin DLLs output under the matching `plugins/` folder next to the exe
(see [Directory Structure](./directory-structure.md) and [Build & Config](./build-config.md)).

## Writing / modifying a plugin

- Start from `PluginDemo/` — it shows the export pattern, options dialog,
  custom-draw item, and a system-date/time item.
- Bump `GetAPIVersion()` only if you change `PluginInterface.h`. Each past bump
  is recorded in the update log at the bottom of that file.
- Do not introduce a second plugin/loading abstraction beside `CPluginManager`.

## Host side: plugin item ordering

When the host folds plugin items into `CommonDisplayItem`, ordering and
per-item visibility become persisted settings (see
[Settings & Strings](./settings-strings.md)). Treat plugin item add/remove as a
settings migration concern.

Reference files:
- `include/PluginInterface.h` — the ABI (authoritative).
- `TrafficMonitor/PluginManager.cpp`, `TrafficMonitor/PluginManager.h`
- `TrafficMonitor/DisplayItem.cpp`, `TrafficMonitor/DisplayItem.h`
- `PluginDemo/PluginDemo.cpp`, `PluginDemo/PluginDemo.h`,
  `PluginDemo/CustomDrawItem.cpp/.h`
