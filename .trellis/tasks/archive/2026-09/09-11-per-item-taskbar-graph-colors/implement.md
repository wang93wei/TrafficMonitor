# Implement — Per-item taskbar graph colors

> Retrospective execution record. The work was performed on a separate clone of
> the fork and delivered as a pull request, so the ordered checklist below
> documents the actual sequence and its validation rather than a forward plan.

## Context

- Fork baseline: `72c717d` (`backup-master`)
- Work branch: `feature/per-item-taskbar-graph-colors`
- Source commits: `6b46016`, `061b67b`
- Delivered PR: https://github.com/wang93wei/TrafficMonitor/pull/1

## Steps

1. **Clone the fork into a separate folder**
   - `git clone https://github.com/wang93wei/TrafficMonitor.git TrafficMonitor-wang93wei`
   - Kept the original project clone untouched so the source commits could be
     fetched locally from it.

2. **Fetch and cherry-pick the source commits**
   - `git fetch ../TrafficMonitor feature/per-item-taskbar-graph-colors`
   - `git cherry-pick 6b46016 061b67b`

3. **Resolve conflicts**
   - Only `TrafficMonitor/language.h` conflicted: the fork had added
     `IDS_GPU_MEMORY_USAGE` / `IDS_GPU_MEMORY_DISP` at the insertion point.
   - Resolution kept both sides (`IDS_COLOR_GRAPH` added alongside the fork's
     GPU keys) — no fork change dropped.
   - `TrafficMonitor.rc` merged as a binary resource; verified it matches the
     source feature's version.

4. **Restore the dropped UTF-8 BOM**
   - The cherry-pick lost the BOM on `language.h` at column 1. Restored it in a
     separate commit so the unrelated encoding change stayed reviewable.

5. **Verify the port**
   - `git diff --check` clean; diff reviewed against the source commits.
   - Static integration review focused on the fork-specific surfaces: GPU VRAM
     item enumeration/serialization, resource IDs, and rendering dispatch. No
     fork-specific integration defects found.
   - Confirmed both `TryDrawStatusBar` and `TryDrawGraph` call sites resolve
     per-item colors for built-in and plugin items.

6. **Build**
   - Lite: `msbuild TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=x64|x86 -p:platformToolset=v143`
   - Full: `msbuild TrafficMonitor.sln -p:configuration=Release -p:platform=x64|x86 -p:platformToolset=v143`
   - All four succeeded (warnings only). `-m:1` was used after the default
     multi-node builds exited unsuccessfully with no diagnostic errors.
   - Full builds are the ones that ship built-in temperature items and link
     `OpenHardwareMonitorApi.dll`; dependency inspection confirmed the linkage
     and the managed deps (`LibreHardwareMonitorLib.dll`, `HidSharp.dll 2.1.0`).

7. **Manual runtime verification**
   - Contributor ran the full x64 build on Windows and confirmed the feature
     works.

8. **Open the PR**
   - Pushed to `ObnubiladO:wang93wei/per-item-taskbar-graph-colors` and opened
     PR #1 against `wang93wei/TrafficMonitor:backup-master`.
   - No binaries, build logs, or local config included.

9. **Address maintainer review**
   - Maintainer asked for Trellis task records and relevant spec updates.
   - Added this task record and updated
     `.trellis/spec/cpp-mfc/drawing-taskbar.md` and
     `.trellis/spec/cpp-mfc/settings-strings.md`.

## Validation commands

```text
msbuild TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=x64 -p:platformToolset=v143 -m:1
msbuild TrafficMonitor_Lite.sln -p:configuration="Release (lite)" -p:platform=x86 -p:platformToolset=v143 -m:1
msbuild TrafficMonitor.sln -p:configuration=Release -p:platform=x64 -p:platformToolset=v143 -m:1
msbuild TrafficMonitor.sln -p:configuration=Release -p:platform=x86 -p:platformToolset=v143 -m:1
```

## Review gates

- [x] Port diff reviewed against the source commits
- [x] Fork-specific surfaces reviewed for integration conflicts
- [x] Full and Lite builds pass for x64 and x86
- [x] Manual runtime confirmation
- [x] Specs updated for the new settings/rendering seams

## Rollback points

- Before `git cherry-pick` — trivial, no local change yet.
- After the cherry-picks — `git revert` of the feature commits; the change is
  additive and leaves no required migration (see `design.md`).

## Follow-ups (not in scope)

- Stale graph-color INI keys are never pruned (documented in `prd.md` and
  `settings-strings.md`).
- ARM64EC was not built locally; CI covers it for Lite.
