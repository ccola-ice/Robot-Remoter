param([string]$Compiler = 'gcc')
$ErrorActionPreference = 'Stop'
$touchTestRoot = $PSScriptRoot
$touchTestOutput = Join-Path ([IO.Path]::GetTempPath()) 'remoter-touch-host-test.exe'
& $Compiler '-std=c99' '-O0' '-g' '-Wall' '-Wextra' '-Wno-format' '-Wno-sign-compare' '-Wno-unused-function' '-Wno-unused-variable' '-I' (Join-Path $touchTestRoot 'include') (Join-Path $touchTestRoot 'touch_test.c') '-o' $touchTestOutput
if ($LASTEXITCODE -ne 0) { throw 'Touch regression test compilation failed.' }
& $touchTestOutput
if ($LASTEXITCODE -ne 0) { throw 'Touch regression test failed.' }
$debugTestOutput = Join-Path ([IO.Path]::GetTempPath()) 'remoter-debug-text-test.exe'
$fatFsPath = Join-Path $touchTestRoot '../../3_Protocol/FatFs'
& $Compiler '-std=c99' '-O0' '-Wall' '-Wextra' '-I' $fatFsPath (Join-Path $touchTestRoot 'debug_text_test.c') (Join-Path $fatFsPath 'cc936.c') '-o' $debugTestOutput
if ($LASTEXITCODE -ne 0) { throw 'Debug text test compilation failed.' }
& $debugTestOutput
if ($LASTEXITCODE -ne 0) { throw 'Debug text test failed.' }
