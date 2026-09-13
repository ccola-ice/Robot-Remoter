param([string]$Compiler = 'gcc')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$temp = Join-Path ([IO.Path]::GetTempPath()) ('remoter-lcd-page-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temp) | Out-Null
function Get-Functions([string]$source, [string[]]$names) {
    $bodies = @()
    $prototypes = @()
    foreach($name in $names) {
        $pattern = '(?ms)^(?:static )?(?:__inline )?(?:void|uint16_t|int16_t|sFONT\s*\*)\s+' + $name + '\s*\([^;]*?\)\s*\{.*?^\}'
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
    $drawFunctions = Get-Functions $driver @('LCD_Draw_Rect', 'ILI9806G_OpenWindow', 'ILI9806G_SetCursor',
        'ILI9806G_FillColor', 'ILI9806G_Clear', 'ILI9806G_SetPointPixel', 'ILI9806G_DrawPoint',
        'ILI9806G_GetPointPixel', 'ILI9806G_DrawLine', 'ILI9806G_DrawRectangle', 'ILI9806G_Fill',
        'ILI9806G_DrawCircle', 'ILI9806G_DispChar_EN', 'ILI9806G_DispString_EN',
        'LCD_SetFont', 'LCD_SetTextColor', 'LCD_SetBackColor')
    $forward = 'void ILI9806G_OpenWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);'
    [IO.File]::WriteAllText((Join-Path $temp 'lcd_page_driver.inc'), ($forward + [Environment]::NewLine + $block + $drawFunctions))
    $gui = [IO.File]::ReadAllText((Join-Path $repo '1_App/gui.c'), $enc)
    $mapping = ([regex]::Matches($gui, '(?m)^#define ROBOT_\w+[^\r\n]*') | ForEach-Object { $_.Value }) -join [Environment]::NewLine
    $guiFunctions = Get-Functions $gui @('gui_prepare_page', 'gui_clear_page_band', 'gui_clear_page_content',
        'gui_update_progress_bar', 'gui_draw_channel_card', 'main_menu', 'channel_monitor_page',
        'gui_robot_card', 'gui_robot_stick_value', 'gui_robot_draw_stick', 'robot_control_page')
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
static void FLASH_SPI_Init(void) {}
static void FLASH_Read_Data(uint8_t *buffer, unsigned address, unsigned size)
{ (void)address; while(size--) *buffer++ = 0U; }
'@)
    $exe = Join-Path $temp 'lcd-page-test.exe'
    & $Compiler '-std=c99' '-O2' '-Wall' '-Wextra' '-Werror' '-Wno-sign-compare' '-finput-charset=GBK' '-fexec-charset=GBK' '-I' $temp '-I' (Join-Path $repo '1_App') '-I' (Join-Path $repo '5_ModuleDrivers/fonts') (Join-Path $PSScriptRoot 'lcd_page_test.c') (Join-Path $repo '5_ModuleDrivers/fonts/fonts.c') '-o' $exe
    if($LASTEXITCODE -ne 0) { throw 'LCD page test compilation failed.' }
    & $exe
    if($LASTEXITCODE -ne 0) { throw 'LCD page tests failed.' }
} finally {
    $resolved = [IO.Path]::GetFullPath($temp)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if(!$resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe temporary path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
