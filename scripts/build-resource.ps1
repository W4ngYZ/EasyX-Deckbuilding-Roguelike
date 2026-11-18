# Build res/resource.rc into build/resource.o.
# resource.rc is kept as UTF-8 so it stays readable, but windres opens asset
# files through the ANSI code page on Windows: a UTF-8 script that references
# Chinese file names (res/assets/enemies/*.png) fails with "Invalid argument".
# So the script is first transcoded to GBK cp936 and then handed to windres.
# windres resolves rc paths against the cwd, which is the repo root ($root),
# so resource.rc references assets as "res/assets/..." relative to the root.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$rcUtf8 = Join-Path $root 'res\resource.rc'
$hdr = Join-Path $root 'res\resource.h'
$buildDir = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
# The compile task outputs exes into bin/, but g++ never creates the directory.
New-Item -ItemType Directory -Force -Path (Join-Path $root 'bin') | Out-Null

$rcGbk = Join-Path $buildDir 'resource.gbk.rc'
$objPath = Join-Path $buildDir 'resource.o'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$text = [System.IO.File]::ReadAllText($rcUtf8, $utf8NoBom)
[System.IO.File]::WriteAllText($rcGbk, $text, [System.Text.Encoding]::GetEncoding(936))

# #include "resource.h" resolves next to the generated script, so a plain copy
# avoids passing -I (its absolute path contains a space that gcc would split).
Copy-Item -LiteralPath $hdr -Destination (Join-Path $buildDir 'resource.h') -Force

# windres calls the preprocessor bare and without -E by default, which fails
# here (MSYS2 lives under "D:/Program Files"). Declare both explicitly.
# --use-temp-file spawns the preprocessor directly instead of through cmd,
# so the quoted program path survives.
$gcc = 'D:/Program Files/msys2/mingw64/bin/gcc.exe'
Push-Location $root
try {
    & 'D:\Program Files\msys2\mingw64\bin\windres.exe' `
        '--use-temp-file' `
        ('--preprocessor=' + $gcc) `
        '--preprocessor-arg=-E' '--preprocessor-arg=-x' '--preprocessor-arg=c-header' `
        '-c' '936' '-i' $rcGbk '-o' $objPath
    if ($LASTEXITCODE -ne 0) { throw "windres failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}
Write-Output "resource ok: $objPath"
