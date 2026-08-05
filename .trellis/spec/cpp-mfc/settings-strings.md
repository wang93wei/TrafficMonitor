# Settings & Strings

> Persisted settings models, INI I/O, and localized user-visible text.

---

## Settings persistence model

Settings are modeled as plain structs in `TrafficMonitor/CommonData.h` and
read/written through the `CSettingsHelper` / `CIniHelper` pair.

- `CIniHelper` (`TrafficMonitor/IniHelper.h`) — low-level INI read/write.
- `CSettingsHelper : public CIniHelper` (`TrafficMonitor/SettingsHelper.h`) —
  typed accessors over the settings structs.

Persisted struct examples in `TrafficMonitor/CommonData.h`:

| Struct | Owns |
|--------|------|
| `MainConfigData` | Top-level config (paths, portable flag) |
| `GeneralSettingData` | General options |
| `MainWndSettingData` / `TaskBarSettingData` (both extend `PublicSettingData`) | Per-window display settings |
| `SkinSettingData` | Skin selection |
| `FontInfo`, `DispStrings`, `StringSet` | Fonts and display text |
| `Date`, `HistoryTraffic : public Date` | Traffic history entries |

**Do not add a second settings/persistence abstraction.** Add new persisted
fields to the relevant struct in `CommonData.h`, then add read/write accessors
in `CSettingsHelper`.

## Options dialog flow

`COptionsDlg` (`TrafficMonitor/OptionsDlg.*`) copies settings struct data into
`theApp`, notifies plugins, and updates / reopens taskbar windows when needed.
When adding a setting that affects the taskbar or plugins, wire both halves
(notify plugins, reopen taskbar) — see `WM_SETTINGS_APPLIED` (`stdafx.h:73`).

## Runtime config files (portable mode)

Runtime files are expected beside `TrafficMonitor.exe`:

| File | Purpose |
|------|---------|
| `config.ini` | Main configuration |
| `global_cfg.ini` | Global config (e.g. portable-mode flag) |
| `history_traffic.dat` | History traffic data |
| `skins/`, `plugins/`, `language/` | Skins, plugin DLLs, localization INIs |

Non-writable installs fall back to `%APPDATA%\TrafficMonitor`. When adding code
that reads/writes a runtime file, use the existing path resolution
(`CTrafficMonitorApp` derives paths at startup) rather than hardcoding
`.\filename`.

## Localized text

User-visible strings must go through `CCommon::LoadText` / `CStrTable`
(`TrafficMonitor/Common.h:154`) and live in `TrafficMonitor/language/*.ini`, with
resource IDs in `TrafficMonitor/resource.h`.

```text
TrafficMonitor/Common.h:154    static CString LoadText(const wchar_t* id, ...)
TrafficMonitor/Common.cpp:932  CString CCommon::LoadText(...)  // delegates to theApp.m_str_table
TrafficMonitor/Common.h:161    static CString LoadTextFormat(id, {params})  // formatted variant
```

Rules:
- Never hardcode English (or any) user-facing literal in source.
- Add the string key to `TrafficMonitor/language/*.ini` and the `IDS_*` to
  `resource.h`.
- Log/diagnostic strings that are never shown to the user are exempt.

Reference files:
- `TrafficMonitor/CommonData.h`, `TrafficMonitor/SettingsHelper.h`,
  `TrafficMonitor/IniHelper.h`, `TrafficMonitor/StrTable.h`
- `TrafficMonitor/Common.h`, `TrafficMonitor/Common.cpp`
- `TrafficMonitor/OptionsDlg.cpp/.h`, `TrafficMonitor/resource.h`
