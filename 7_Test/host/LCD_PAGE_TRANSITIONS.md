# Buffered menu transitions

Page changes compose the new background, widgets and clock in external SRAM.
The old LCD GRAM remains untouched until `menu_process()` calls `LCD_EndPage()`
after `gui_clock_overlay()`. Presentation writes one complete RGB565 image,
without a visible white clear, backlight toggle or interleaved widget drawing.
Regular live updates still draw directly to the LCD after presentation.

If the SRAM buffer is disabled, `LCD_BeginPage()` clears the entire visible
page to the requested background before direct rendering. This fallback also
clears unpainted gaps and old text. `LCD_BlitRGB565()` supports both buffered
and direct drawing, clipping right/bottom edges while preserving source stride.

Hardware Test and EEPROM service screens now begin composition in
`diag_screen()` and present the complete page before `diag_key()` or
`diag_release()` polls input. Their initial layout is drawn before waiting
for the entry key to be released. Hardware Test also draws its list before
waiting after a test result. Returning home uses the normal buffered path.
Unused margins are initialized by the full background fill.

Normal navigation uses MultiButton's debounced PRESS_DOWN (three 10 ms samples),
without waiting for release or the single-click timeout. Service screens use
the same three-sample down-edge behavior. Keys still held on service exit are
marked consumed before normal button polling resumes. A second press held
inside the click timeout no longer resets the state machine to idle and emits
duplicate down events. Input handling runs before IMU/GPS work in the main loop.

The buffer reserves `[0x6C000000, 0x6C0BB800)` (768,000 bytes / 750 KiB) of the
board's 1 MiB external SRAM. It is enabled only after the SRAM boot test passes.
No linker RAM region or heap is moved. If that test fails, rendering stays on
the direct path. Future external SRAM allocations must avoid the reserved area.

Run the pixel regression from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 7_Test/host/run_lcd_page_tests.ps1
```

The test compiles the actual driver drawing functions, fonts, and main menu,
channel monitor and robot pages against a simulated LCD bus. Repeated page
changes must leave the previous panel image intact during composition, then
match direct rendering pixel for pixel after one full transfer. It also checks
late overlays, edge clipping, SRAM guards, portrait dimensions, direct fallback,
and subsequent live writes. The existing analog tests remain separate.
The regression also replays Hardware Test entry with OK held, verifies its
entire screen is presented before any release wait, and exits with BACK held.
It exercises the actual diagnostic layout and LCD bus with mocked glyph shapes;
it does not verify Chinese glyph appearance. Diagnostic key bounce and held-key
suppression are checked alongside this replay.

`run_ui_input_tests.ps1` compiles the production button library and callbacks
and checks press-edge response, bounce, rapid second presses held down, held
keys on service return, and the menu's 20 ms analog-page refresh schedule.

The latency revision batches solid rectangle fills into SRAM row spans and
decodes fully visible ASCII glyphs directly into destination rows. It avoids
the generic pixel-stream bounds/stride calculations for every background or
font pixel. Clipped glyphs and partial-window writes retain the checked path.
The regression now covers all printable ASCII characters in all three built-in
font sizes, clipped fills/glyphs, and partial-window writes.

After each page transfer the debug UART reports
`[LCD] page compose=... us, transfer=... us`, measured with the already-enabled
DWT cycle counter. Composition includes background initialization; transfer
includes the LCD window setup and full pixel stream. Logging happens after
presentation. Host tests use a mocked counter and cannot establish board latency.

Reference reviewed: the supplied `XRC4.1-008` archive's `USER/main.c` routes its
normal menus directly (e.g. `planner()` uses `first_splash` for static content).
`USER/lvgl_main.c` is a separate demonstration entered via menu 73. Its
`LVGL/lvgl_driver/lv_port_disp.c` flushes the requested area using
`LCD_Color_Fill()`. The LCD driver streams color spans directly. These paths
motivate batching pixel writes here; no reference source was copied into the
firmware, and its board-specific LCD timings were not transferred.

This is software composition followed by an 8080/FSMC transfer, not an atomic
hardware framebuffer swap. It removes intermediate erased/partly drawn frames,
but has no TE synchronization; a scan boundary during the final transfer is
still possible. Host tests do not measure panel scanout. On the board, check
rapid Home -> Channel -> Home and Home -> Robot -> Home, stationary joystick
dots after entry, and the clock. Confirm SRAM reports PASS at startup.
