param ($vcpkgDir, $vcpkgTriplet = "x64-windows-static", $innoSetupExePath)

$location = Get-Location

if ($null -eq $vcpkgdir) {
    # Try to find vcpkg in PATH
    $vcpkgExe = Get-Command vcpkg.exe -ErrorAction SilentlyContinue

    if ($null -eq $vcpkgExe) {
        Write-Error "vcpkg.exe not found in PATH. Please specify the path to vcpkg.exe directory using the -vcpkgDir parameter."
        exit 1
    }

    $vcpkgDir = [System.IO.Path]::GetDirectoryName($vcpkgExe.Path)
}

# Try to find ISCC.exe in PATH
if ($null -eq $innoSetupExePath) {
    $innoSetupExePath = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Path

    if ($null -eq $innoSetupExePath) {
        Write-Warning "ISCC.exe not found in PATH. Installer will not be created. To specify the path to iscc.exe, use the -innoSetupExePath parameter."
    }
    else {
        $innoSetupExePath = [System.IO.Path]::GetDirectoryName($innoSetupExePath) + "/ISCC.exe"
    }
}

$drawpileDir = (Resolve-Path -Path "$PSScriptRoot/../../")
$buildDir = "$drawpileDir/build-win-msvc"
if (!(Test-Path $buildDir)) {
    Write-Error "$buildDir does not exist. Make sure to run build-release.ps1 first."
    exit 1
}

$drawpileVersion = Get-Content "$buildDir/DRAWPILE_VERSION.txt"

$qtTranslationsDir = "$vcpkgDir/installed/$vcpkgTriplet/share/qt5/translations"
Write-Output "Using translations files from $qtTranslationsDir"

$outDir = "$PSScriptRoot/drawpile-win-$drawpileVersion"
Remove-Item "$outDir" -Recurse -Force -ErrorAction:SilentlyContinue
New-Item -Path $outDir -ItemType Directory -Force

# Copy binaries
Copy-Item -Path "$buildDir/bin/drawpile.exe" -Destination $outDir
Copy-Item -Path "$buildDir/bin/drawpile-srv.exe" -Destination $outDir

# Copy resources
Copy-Item -Path "$drawpileDir/desktop/palettes" -Destination $outDir -Recurse
Copy-Item -Path "$drawpileDir/desktop/theme" -Destination $outDir -Recurse
Copy-Item -Path "$drawpileDir/desktop/sounds" -Destination $outDir -Recurse
Copy-Item -Path "$drawpileDir/desktop/nightmode.colors" -Destination $outDir
Copy-Item -Path "$drawpileDir/desktop/initialbrushpresets.db" -Destination $outDir

# Copy translations
New-Item -Path "$outDir/i18n" -ItemType Directory -Force
foreach ($tr in ("cs", "de", "fi", "fr", "it", "ja", "pt", "ru", "uk", "vi", "zh")) {
    Copy-Item -Path "$qtTranslationsDir/qt_$tr.qm" -Destination "$outDir/i18n/"-ErrorAction SilentlyContinue
    Copy-Item -Path "$buildDir/src/libclient/drawpile_$tr.qm" -Destination "$outDir/i18n/"-ErrorAction SilentlyContinue
}

# Copy text files
# Get-Content|Out-File converts LF to CRLF
Get-Content -Path "$drawpileDir/ChangeLog" | Out-File -FilePath "$outDir/Changelog.txt" 
Get-Content -Path "$drawpileDir/README.md" | Out-File -FilePath "$outDir/Readme.txt" 
Get-Content -Path "$drawpileDir/AUTHORS" | Out-File -FilePath "$outDir/Authors.txt" 
Get-Content -Path "$drawpileDir/COPYING" | Out-File -FilePath "$outDir/License.txt"

# Zip it up
$zipPath = "$PSScriptRoot/drawpile-win-$drawpileVersion.zip"
Compress-Archive -Path $outDir -DestinationPath $zipPath -CompressionLevel "Optimal" -Force
Write-Output "Created $zipPath"

# Create installer
if ($null -ne $innoSetupExePath) {
    Write-Output "Creating installer using $innoSetupExePath"

    $installerFilename = "drawpile-win-$drawpileVersion-setup"

    Get-Content "$PSScriptRoot/drawpile.iss" |
    ForEach-Object { $_ -replace "DRAWPILE_VERSION", $drawpileVersion } |
    ForEach-Object { $_ -replace "SOURCE_DIR", $outDir } |
    ForEach-Object { $_ -replace "OUTPUT_DIR", $PSScriptRoot } |
    ForEach-Object { $_ -replace "OUPUT_FILENAME", $installerFilename } |
    Out-File -FilePath "$PSScriptRoot/drawpile-out.iss" -Encoding ascii

    & $innoSetupExePath "$PSScriptRoot/drawpile-out.iss"

    Remove-Item "$PSScriptRoot/drawpile-out.iss"

    Write-Output "Created $PSScriptRoot/$installerFilename.exe"
}

# Clean up
Remove-Item $outDir -Recurse -Force

Set-Location -Path $location
