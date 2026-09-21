param([string]$Compiler = 'gcc')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$temp = Join-Path ([IO.Path]::GetTempPath()) ('remoter-ui-input-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temp) | Out-Null
try {
    foreach($file in @('multi_button.c', 'multi_button.h', 'multi_button_user.c', 'multi_button_user.h')) {
        Copy-Item -LiteralPath (Join-Path $repo ('5_Middleware/MultiButton/' + $file)) -Destination $temp
    }
    Copy-Item -LiteralPath (Join-Path $repo '1_App/menu.h') -Destination $temp
    [IO.File]::WriteAllText((Join-Path $temp 'stm32f4xx.h'), '#include <stdint.h>')
    [IO.File]::WriteAllText((Join-Path $temp 'bsp_SysTick.h'), '')
    [IO.File]::WriteAllText((Join-Path $temp 'bsp_gpio_button.h'), @'
#define BUTTON_OK_GPIO_PORT 0U
#define BUTTON_BACK_GPIO_PORT 0U
#define BUTTON_LEFT_GPIO_PORT 0U
#define BUTTON_RIGHT_GPIO_PORT 0U
#define BUTTON_OK_PIN 0U
#define BUTTON_BACK_PIN 1U
#define BUTTON_LEFT_PIN 2U
#define BUTTON_RIGHT_PIN 3U
'@)
    $source = [IO.File]::ReadAllText((Join-Path $repo '1_App/menu.c'), [Text.Encoding]::GetEncoding(28591))
    $tick = [regex]::Match($source, '(?ms)^void menu_tick_10ms\(void\).*?^\}').Value
    $constants = ([regex]::Matches($source, '(?m)^#define (?:MENU_REFRESH_TICKS|CLOCK_REFRESH_TICKS)[^\r\n]*') | ForEach-Object { $_.Value }) -join [Environment]::NewLine
    if(!$tick -or !$constants) { throw 'Missing production menu timer.' }
    [IO.File]::WriteAllText((Join-Path $temp 'menu_tick.inc'), ($constants + [Environment]::NewLine + $tick))
    $exe = Join-Path $temp 'ui-input-test.exe'
    & $Compiler '-std=c99' '-O2' '-Wall' '-Wextra' '-Werror' '-I' $temp (Join-Path $PSScriptRoot 'ui_input_test.c') (Join-Path $temp 'multi_button.c') '-o' $exe
    if($LASTEXITCODE -ne 0) { throw 'UI input test compilation failed.' }
    & $exe
    if($LASTEXITCODE -ne 0) { throw 'UI input tests failed.' }
} finally {
    $resolved = [IO.Path]::GetFullPath($temp)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if(!$resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe temporary path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
