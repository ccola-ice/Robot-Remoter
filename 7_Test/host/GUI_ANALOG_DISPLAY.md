# Analog display regression checks

Run from the repository root (MinGW GCC required):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 7_Test/host/run_gui_analog_tests.ps1
```

The runner compiles the production display filter and extracts the complete
channel/robot rendering functions from `1_App/gui.c`, with mocked LCD writes.
It checks noise rejection, step response, ADC endpoints, channel mapping,
numeric refresh cadence, page re-entry, immediate link transitions, and that
stationary joystick dots are not erased when ADC values change.

Display settings are in `1_App/gui_analog_filter.h`: changes of at least 32 ADC
counts follow the current sample immediately; smaller changes use 1/2 (16 to
32 counts) or 1/4 smoothing. An 8-count hysteresis suppresses minor fluctuations.
Both pages now request a refresh every 20 ms. Channel and joystick numbers
update on that frame whenever the filtered value changes, with no additional
numeric refresh gate. Large steps and reversals must reach the new value on
the very next rendering call. Actual wall time also depends on main-loop work.
The filter affects display values only. Remote robot telemetry keeps its
200 ms limit (10 frames); link changes remain immediate. Dot motion uses a
two-pixel hysteresis in `gui_robot_draw_stick`.

On hardware, verify both pages with the sticks released, slow movements,
full-travel movements, and repeated page changes. Confirm stationary dots
remain visible and moving dots leave no trails. Host checks cannot verify
LCD scanout flicker or the actual board's ADC noise amplitude.
