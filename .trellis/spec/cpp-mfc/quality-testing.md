# Quality & Testing

> What "done" means here, and how to verify a change without an integrated test
> project.

---

## Test reality

- There is **no integrated test project** and **no CI test step**. CI
  (`.github/workflows/main.yml`) only builds Lite release artifacts.
- Standalone tests live in `tests/` as plain `int main()` programs using
  `EXIT_FAILURE` on assertion mismatch — no test framework.
- `_DEBUG` startup runs `CTest::Test()` (`TrafficMonitor/Test.cpp`), which is
  ad hoc/debug-only and **not** a substitute for regression tests.

## Standalone pure test (the only automated check)

From a VS 2022 Developer Command Prompt:

```bat
cl /EHsc /std:c++20 /I. tests\gpu_memory_selection_test.cpp
gpu_memory_selection_test.exe
```

The test exits non-zero and prints the failing case name on mismatch. Extend
`tests/gpu_memory_selection_test.cpp` for new
`GpuMemorySelection::SelectPreferredAdapterMemoryLimit` edge cases. The pattern
(`ExpectPreferredLimit` / `ExpectNoLimit` helpers) is the model for any new
header-only pure-logic test.

## When to add a standalone test

Add a `tests/<feature>_test.cpp` when you add or change **pure, header-only
logic** with clear inputs/outputs (selection algorithms, parsing, math). UI,
drawing, taskbar, plugin, and hardware-acquisition code is not unit-testable
in this setup — verify it by build + manual smoke check instead.

## Build the narrowest relevant config

Don't build every config for every change. Pick the minimum that exercises your
change:

| Change | Build |
|--------|-------|
| Pure logic in `PdhHardwareQuery/` (header-only) | Standalone test, plus a Lite x64 build |
| Main-window UI / drawing | Lite x64 (or full x64 if you touch skin/draw primitives) |
| Temperature / `OpenHardwareMonitorApi` | Full x64 (`TrafficMonitor.sln`, release) |
| Taskbar / DPI / multi-monitor | Both Lite x64 and the relevant taskbar path |
| Plugin ABI (`include/PluginInterface.h`) | Full x64 **and** `PluginDemo` |

See [Build & Config](./build-config.md) for commands.

## Manual smoke checklist (Windows)

Before marking done, verify on a real Windows machine as relevant:

- [ ] App launches and the floating window paints.
- [ ] Taskbar window **opens** and **reopens** (toggle in options; trigger a reopen via `WM_REOPEN_TASKBAR_WND`).
- [ ] DPI change / display reconfiguration does not crash or duplicate the taskbar window.
- [ ] Language switch updates all user-visible strings (no hardcoded literals leaked through).
- [ ] Plugin loads from `plugins/` and its display item renders; `GetItemValueText()` does not stall the UI.
- [ ] Hardware unavailable / error path: no misleading totals, temperature shows nothing when unavailable (Lite build has no temperature).
- [ ] No modal UI appears from the worker thread under load.

## Verification rule for AI cross-review findings

When acting on automated review output, verify each CRITICAL/WARNING against the
actual code before acting. Budget a meaningful false-positive rate for findings
that confuse trust boundaries (e.g. treating bundled INI resources as untrusted
external input) or ignore documented design intent in code comments. See
[`../guides/index.md`](../guides/index.md) for the full list of AI-reviewer
false-positive patterns.

Reference files:
- `tests/gpu_memory_selection_test.cpp`
- `TrafficMonitor/Test.cpp` (debug-only, not a regression suite)
- `.github/workflows/main.yml` (build-only CI)
