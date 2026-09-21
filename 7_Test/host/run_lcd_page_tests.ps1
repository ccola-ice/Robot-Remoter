param([string]$Compiler = 'gcc', [string]$PreviewDirectory = '', [string]$PreviewFont = '')
$ErrorActionPreference = 'Stop'
if($PreviewFont -and !(Test-Path -LiteralPath $PreviewFont -PathType Leaf)) { throw "Preview font not found: $PreviewFont" }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$temp = Join-Path ([IO.Path]::GetTempPath()) ('remoter-lcd-page-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temp) | Out-Null
function Get-Functions([string]$source, [string[]]$names) {
    $bodies = @()
    $prototypes = @()
    foreach($name in $names) {
        $pattern = '(?ms)^(?:static )?(?:__inline )?(?:void\s+|uint8_t\s+|uint16_t\s+|int16_t\s+|sFONT\s*\*\s*|const char\s*\*\s*)' + $name + '\s*\([^;]*?\)\s*\{.*?^\}'
        $matches = [regex]::Matches($source, $pattern)
        if($matches.Count -ne 1) { throw "Expected one function: $name" }
        $body = $matches[0].Value
        $prototypes += $body.Substring(0, $body.IndexOf('{')).Trim() + ';'
        $bodies += $body
    }
    ($prototypes + $bodies) -join [Environment]::NewLine
}
try {
    $enc = [Text.Encoding]::GetEncoding(28591)
    $driver = [IO.File]::ReadAllText((Join-Path $repo '5_ModuleDrivers/bsp_fsmc_lcd.c'), $enc)
    $start = $driver.IndexOf('/* Page transitions reserve')
    $end = $driver.IndexOf('///**', $start)
    $block = $driver.Substring($start, $end - $start)
    $drawFunctions = Get-Functions $driver @('LCD_Draw_Rect', 'ILI9806G_GramScan', 'ILI9806G_OpenWindow', 'ILI9806G_SetCursor',
        'ILI9806G_FillColor', 'ILI9806G_Clear', 'ILI9806G_SetPointPixel', 'ILI9806G_DrawPoint',
        'ILI9806G_GetPointPixel', 'ILI9806G_DrawLine', 'ILI9806G_DrawRectangle', 'ILI9806G_Fill',
        'ILI9806G_DrawCircle', 'ILI9806G_DispChar_EN', 'ILI9806G_DispString_EN', 'LCD_DispString_EN_Bold',
        'LCD_SetFont', 'LCD_SetTextColor', 'LCD_SetBackColor')
    $forward = 'void ILI9806G_OpenWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h); static __inline void ILI9806G_FillColor(uint32_t count, uint16_t color);'
    $drawFunctions = $drawFunctions.Replace('ILI9806G_DispString_EN', 'lcd_real_DispString_EN')
    [IO.File]::WriteAllText((Join-Path $temp 'lcd_page_driver.inc'), ($forward + [Environment]::NewLine + $block + $drawFunctions))
    $gui = [IO.File]::ReadAllText((Join-Path $repo '1_App/gui.c'), $enc)
    $mapping = ([regex]::Matches($gui, '(?m)^#define ROBOT_\w+[^\r\n]*') | ForEach-Object { $_.Value }) -join [Environment]::NewLine
    $guiFunctions = Get-Functions $gui @('gui_prepare_page', 'gui_clock_overlay',
        'gui_control_status_text', 'gui_dashboard_status', 'menu_group_page', 'main_menu', 'gui_monitor_tabs',
        'channel_monitor_page', 'channel_output_monitor_page',
        'gui_robot_stick_value', 'gui_robot_dot_patch', 'gui_robot_draw_stick', 'gui_robot_format_value', 'robot_control_page',
        'gui_file_decode_utf8', 'gui_file_source_is_utf8', 'gui_file_display_text', 'gui_file_size_text',
        'gui_settings_row', 'gui_settings_scroll', 'gui_file_row_icon',
        'parameter_settings_page', 'nrf_settings_page', 'file_browser_page', 'calendar_page')
    [IO.File]::WriteAllText((Join-Path $temp 'lcd_page_gui.inc'), ($mapping + [Environment]::NewLine + $guiFunctions))
    $diag = [IO.File]::ReadAllText((Join-Path $repo '1_App/diagnostics.c'), $enc)
    $start = $diag.IndexOf('#define DIAG_LINE_CACHE_COUNT')
    $end = $diag.IndexOf('static uint8_t confirm(', $start)
    $diagCode = $diag.Substring($start, $end - $start)
    $start = $diag.IndexOf('#define TEST_COUNT')
    $diagCode += $diag.Substring($start)
    [IO.File]::WriteAllText((Join-Path $temp 'lcd_page_diag.inc'), $diagCode, $enc)
    $names = [regex]::Match($diag, '(?s)static const char \* const names\[TEST_COUNT\] = .*?;').Value
    if(!$names) { throw 'Missing diagnostic menu labels.' }
    [IO.File]::WriteAllText((Join-Path $temp 'lcd_diag_names.inc'), $names, $enc)
    [IO.File]::WriteAllText((Join-Path $temp 'stm32f4xx.h'), '#include <stdint.h>')
    [IO.File]::WriteAllText((Join-Path $temp 'bsp_usart_debug.h'), '')
    [IO.File]::WriteAllText((Join-Path $temp 'bsp_spi_flash.h'), @'
void FLASH_SPI_Init(void);
void FLASH_Read_Data(uint8_t *buffer, unsigned address, unsigned size);
'@)
    $exe = Join-Path $temp 'lcd-page-test.exe'
    & $Compiler '-std=c99' '-O2' '-Wall' '-Wextra' '-Werror' '-Wno-sign-compare' '-finput-charset=GBK' '-fexec-charset=GBK' '-I' $temp '-I' (Join-Path $repo '1_App') '-I' (Join-Path $repo '5_SystemDrivers') '-I' (Join-Path $repo '5_ModuleDrivers/fonts') (Join-Path $PSScriptRoot 'lcd_page_test.c') (Join-Path $repo '5_ModuleDrivers/fonts/fonts.c') '-o' $exe
    if($LASTEXITCODE -ne 0) { throw 'LCD page test compilation failed.' }
    if ($PreviewDirectory) {
        [IO.Directory]::CreateDirectory([IO.Path]::GetFullPath($PreviewDirectory)) | Out-Null
        if($PreviewFont) { & $exe ([IO.Path]::GetFullPath($PreviewDirectory)) ([IO.Path]::GetFullPath($PreviewFont)) }
        else { & $exe ([IO.Path]::GetFullPath($PreviewDirectory)) }
    } else { & $exe }
    if($LASTEXITCODE -ne 0) { throw 'LCD page tests failed.' }
    if ($PreviewDirectory) {
        Add-Type -AssemblyName System.Drawing
        Get-ChildItem -LiteralPath ([IO.Path]::GetFullPath($PreviewDirectory)) -Filter '*.bmp' | ForEach-Object {
            $bitmap = [Drawing.Image]::FromFile($_.FullName)
            try {
                if ($bitmap.Width -ne 800 -or $bitmap.Height -ne 480) { throw 'Preview dimensions changed.' }
                $bitmap.Save([IO.Path]::ChangeExtension($_.FullName, '.png'), [Drawing.Imaging.ImageFormat]::Png)
            } finally { $bitmap.Dispose() }
        }
    }
} finally {
    $resolved = [IO.Path]::GetFullPath($temp)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if(!$resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe temporary path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
