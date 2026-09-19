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

The channel monitor uses `1_App/gui_analog_filter.h`: changes of at least 32
ADC counts follow the current sample immediately; smaller changes use 1/2 or
1/4 smoothing and an 8-count hysteresis.

The robot control page uses `1_App/gui_robot_filter.h`: median-of-three rejects
a single spike, small movements use 1/4 smoothing and a 12-count hysteresis,
and deliberate movements of at least 256 counts settle within two rendering
calls. The displayed normalized axes include the control path's 5% neutral
band. Page entry resets the filter to the current input.

Both pages request live updates every 20 ms. Robot joystick numbers and online
telemetry use a separate 250 ms wall-clock gate (at most 4 Hz), including clock
rollover. Unchanged status and offline telemetry produce no repeated text
writes. Link transitions remain immediate. All robot text is padded and clipped
to its card width; offline cards render their message directly.

Dot motion uses a two-pixel hysteresis and snaps exactly to the center when
neutral. Static axes and outlines are drawn on entry. A movement streams the
final pixels of two 17x17 patches through `LCD_BlitRGB565`, retaining the new
marker in their overlap. The marker center is limited to radius 44 so the
square patches stay inside the radius-56 outline. These filters affect display
values; command generation runs in `control_link.c`.

On hardware, verify both pages with the sticks released, slow movements,
full-travel movements, and repeated page changes. Confirm stationary dots
remain visible and moving dots leave no trails. Host checks cannot verify
LCD scanout flicker or the actual board's ADC noise amplitude.
