# Design — Per-item taskbar graph colors

## Approach

Port the source commits (`6b46016`, `061b67b`) onto the fork baseline `72c717d`
with `git cherry-pick`, resolving conflicts in favor of both sides, then verify
by full build. The port follows the existing settings/rendering seams rather
than introducing new ones:

- persisted state lives in `TaskBarSettingData` (`CommonData.h`) and is read/written
  by `CSettingsHelper` — no new persistence abstraction;
- rendering keeps using `GetUsageGraphColor()` as the single resolution point,
  extended with an item-aware overload so callers do not branch on the setting;
- the color dialog reuses the existing per-item text color table and adds a
  graph column instead of a second dialog.

## Data model

`TrafficMonitor/CommonData.h` — `TaskBarSettingData`:

```cpp
bool specify_each_item_graph_color{ false };            // enable flag
std::map<CommonDisplayItem, COLORREF> graph_colors{};   // per-item colors
COLORREF GetUsageGraphColor() const;                    // global/effective color
COLORREF GetUsageGraphColor(CommonDisplayItem item) const; // item-aware
```

`CommonDisplayItem` is already the key type of the existing per-item text color
map and is hashable/comparable, so it is reused as the map key. The item-aware
overload returns the mapped color when the enable flag is set and an entry
exists, otherwise it delegates to the no-argument overload — so the global color
(including the `graph_color_following_system` accent resolution) stays the single
fallback path.

## Persistence

`TrafficMonitor/SettingsHelper.{h,cpp}` gains a symmetric pair:

```cpp
void LoadTaskbarWndGraphColors(const wchar_t* AppName, std::map<CommonDisplayItem, COLORREF>& graph_colors);
void SaveTaskbarWndGraphColors(const wchar_t* AppName, const std::map<CommonDisplayItem, COLORREF>& graph_colors);
```

- Keys are produced by `CommonDisplayItem::GetItemIniKeyName()`, the same key
  scheme the per-item text colors use, so built-in and plugin items both round
  trip and plugin items stay stable across reordering.
- Loading iterates `theApp.m_plugins.AllDisplayItemsWithPlugins()` and only
  assigns entries that exist in the INI, so items without a saved override are
  simply absent from the map and fall back to the global color.
- Sections: `task_bar_graph_color` for the live settings, and
  `taskbar_default_style_graph_color_<n>` for each taskbar style preset.

`TrafficMonitor/TrafficMonitor.cpp` (`LoadConfig` / `SaveConfig`) reads and
writes `specify_each_item_graph_color` plus the color map next to the existing
`graph_color_following_system` handling.

### Known limitation (inherited, not introduced)

`SaveTaskbarWndGraphColors` only upserts keys — it never deletes keys that are
no longer present in the map, and `CIniHelper::WriteInt` cannot express removal.
Because `CTaskbarDefaultStyle::ApplyDefaultStyle` replaces `graph_colors` with
the preset's map, stale keys can reload after a restart. This mirrors the source
feature; it is documented in the PRD and in
`.trellis/spec/cpp-mfc/settings-strings.md` rather than fixed here.

## Rendering

`TrafficMonitor/TaskBarDlg.{h,cpp}` — the two graph entry points resolve the
color through the new overload:

| Function | Mode | Change |
|----------|------|--------|
| `TryDrawStatusBar(drawer, rect_bar, item, usage_percent)` | bar | now takes the display item; uses `GetUsageGraphColor(item)` for fill and dashed outline |
| `TryDrawGraph(drawer, value_rect, item_type)` | plot | uses `GetUsageGraphColor(item_type)` |

`DrawDisplayItem` passes its `DisplayItem type`, and `DrawPluginItem` passes its
`IPluginItem*`; both convert implicitly to `CommonDisplayItem`, so a single
signature serves built-in and plugin items without overloads. Because color
resolution happens inside these two functions, every taskbar variant
(`CClassicalTaskbarDlg`, `CWin11TaskbarDlg`, `CWineTaskbarDlg`) inherits the
behavior with no per-variant change.

## Settings UI

`TrafficMonitor/TaskBarSettingsDlg.{h,cpp}`:

- New checkbox `IDC_SPECIFY_EACH_ITEM_GRAPH_COLOR_CHECK` and a `CColorStatic`
  preview `IDC_GRAPH_COLOR_STATIC`, placed under the existing graph settings and
  registered in `DoDataExchange` / the message map.
- `DrawStaticColor` renders either the multi-color preview (up to 16 swatches,
  `SetColorNum`) or the single global color.
- `EnableControl` gates the checkbox and preview on graphs being shown
  (`show_status_bar || show_netspeed_figure`) and on the enable flag, matching
  how the existing graph controls are gated.
- `OnBnClickedSpecifyEachItemGraphColorCheck` seeds any missing entries from the
  current effective global color, so newly enabled items start from the global
  color instead of an uninitialized value.
- `IsStyleModified` now also compares `graph_colors` and the enable flag so
  changes are detected for the apply/save path.
- `ApplyDefaultStyle` syncs the checkbox and calls `EnableControl()` so preset
  application cannot leave stale control state.

`TrafficMonitor/TaskbarColorDlg.{h,cpp}` — the per-item color dialog takes the
graph map, the resolved global color, and both enable flags; it exposes a Graph
column and returns both maps via `GetColors()` / `GetGraphColors()`. Both
`IDC_TEXT_COLOR_STATIC3` and `IDC_GRAPH_COLOR_STATIC` open it.

`TrafficMonitor/TaskbarDefaultStyle.{h,cpp}` — `TaskBarStyleData` gains
`graph_colors` and `specify_each_item_graph_color`; `LoadConfig`, `SaveConfig`,
`ApplyDefaultStyle`, and `ModifyDefaultStyle` each handle the new fields.

## Localization & resources

- `TrafficMonitor/language.h` adds `IDS_COLOR_GRAPH`; `resource.h` and
  `TrafficMonitor.rc` add the new control/static IDs and dialog layout.
- `TrafficMonitor/language/English.ini` and `Simplified_Chinese.ini` gain the
  matching string, so `CCommon::LoadText` resolves it in both shipped languages.
- `language.h` keeps its UTF-8 BOM: cherry-picking into a repo whose HEAD
  encoding differed dropped it, which was restored in a follow-up commit.

## Compatibility notes

- No new settings, drawing, or plugin abstraction was added.
- The fork's GPU VRAM item is untouched. It has no graph in the baseline and
  gains none here, so it appears in the color dialog (via
  `AllDisplayItemsWithPlugins`) but draws no graph to color.
- No plugin API surface changed; `CommonDisplayItem` usage stays internal.
- Plugin items are keyed by `GetItemIniKeyName()`, so saved colors follow the
  item rather than its position in the display order.

## Verification

- Full `TrafficMonitor.sln` Release x64 + x86 (MSVC v143, includes
  `OpenHardwareMonitorApi` and temperature support) — succeeded, warnings only.
- Lite Release x64 + x86 — succeeded.
- Static review of the port against the source commits; `git diff --check` clean.
- Manual runtime test on Windows by the contributor.

## Rollback

The change is additive: reverting the feature commits restores the previous
behavior. Settings already written to `config.ini` (`specify_each_item_graph_color`,
`task_bar_graph_color`) are inert on a build without the feature, so no data
migration is required in either direction.
