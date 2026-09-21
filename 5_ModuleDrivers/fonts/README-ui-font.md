Remoter UI is a derived bitmap subset for the existing fixed-size interface.
Chinese cells remain 16x16, 20x20, 32x32 and 48x48. ASCII cells remain half as
wide. The 32px bold style has separate source font weights; no bitmap is scaled
or dilated. Four coverage levels preserve thin lines and smooth curves.

Include `ui_font_data.inc` once from `fonts.c`, and include `ui_font_data.h`
where lookup is needed. `UI_FontBitmap` returns immutable flash storage with
independently padded rows. It neither reads SPI Flash nor allocates RAM.
The caller can use the existing external glyph reader for code points not in
this interface subset. Coverage zero must paint the supplied background so
shorter replacement text cannot leave old pixels.

Regeneration from the EIDE project directory:

```
py -3.9 ../5_ModuleDrivers/fonts/generate_ui_font.py --preview build/ui-clarity/native-font-samples.png
py -3.9 ../5_ModuleDrivers/fonts/generate_ui_font.py --check
py -3.9 ../7_Test/host/run_ui_font_data_tests.py D:/Embedded/mingw64/bin/gcc.exe
```

The generator needs Pillow 11.3.0. Its default package path is the approved
local `build/ui-clarity/python` installation. `--pillow-path`, `--cjk-font`,
`--ascii-font` and `--ascii-bold-font` support other host paths. No original TTF
file belongs in the firmware. Source versions, SHA256 hashes, glyph manifest,
outline sizes, shared baselines and bitmap hashes are recorded in
`ui_font_manifest.json`. Re-run after adding interface strings; `--check`
detects stale assets without rewriting them.

Noto Sans SC regular uses the explicit variable-font weight 500; 32px bold
uses 650. DejaVu Sans Mono supplies regular and bold ASCII. The generator fits
one native outline size per face to the complete fixed cell, retaining all
descenders and punctuation. Chinese and ASCII share one baseline at each size;
individual characters are never vertically centered independently. Source
outline em sizes differ from the fixed cell height because the font metrics
include overshoot and descenders. Each bitmap is rasterized directly at its
selected native size and checked for any clipped ink before packing.

The derived font name is **Remoter UI**. Font assets retain their respective
licenses independently of the surrounding firmware. Distribute
`UI_FONT_LICENSES.txt` with the firmware/source package; it contains the source
copyright notices, full SIL Open Font License 1.1 and full embedded DejaVu
permission notice. Official license sources:

- https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/OFL.txt
- https://dejavu-fonts.github.io/License.html
