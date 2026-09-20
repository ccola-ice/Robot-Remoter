param([string]$Compiler = 'gcc')
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$testDir = Join-Path ([IO.Path]::GetTempPath()) ('remoter-gui-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testDir) | Out-Null
try {
    $source = [IO.File]::ReadAllText((Join-Path $repoRoot '1_App/gui.c'), [Text.Encoding]::GetEncoding(28591))
    $functions = @('gui_monitor_tabs', 'channel_monitor_page', 'channel_output_monitor_page',
        'gui_robot_stick_value', 'gui_robot_dot_patch', 'gui_robot_draw_stick', 'gui_robot_format_value', 'robot_control_page')
    $parts = @([regex]::Matches($source, '(?m)^#define ROBOT_\w+[^\r\n]*') | ForEach-Object { $_.Value })
    foreach ($name in $functions) {
        $pattern = '(?ms)^(?:static )?(?:void|int16_t) ' + $name + '\(.*?^\}'
        $matches = [regex]::Matches($source, $pattern)
        if ($matches.Count -ne 1) { throw "Expected one production function: $name" }
        $parts += $matches[0].Value
    }
    [IO.File]::WriteAllText((Join-Path $testDir 'gui_analog_functions.inc'), ($parts -join [Environment]::NewLine))
    $exe = Join-Path $testDir 'gui-analog-test.exe'
    & $Compiler '-std=c99' '-O2' '-Wall' '-Wextra' '-Werror' '-I' $testDir '-I' (Join-Path $repoRoot '1_App') (Join-Path $PSScriptRoot 'gui_analog_test.c') '-o' $exe
    if ($LASTEXITCODE -ne 0) { throw 'GUI analog test compilation failed.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'GUI analog tests failed.' }
} finally {
    # Only delete the specific temporary directory created above.
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    $resolvedTestDir = [IO.Path]::GetFullPath($testDir)
    if (!$resolvedTestDir.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe temporary path.' }
    Remove-Item -LiteralPath $resolvedTestDir -Recurse -Force
}
