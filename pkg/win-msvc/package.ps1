param ($vcpkgDir, $vcpkgTriplet = "x64-windows-static")

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

$drawpileDir = (Resolve-Path -Path "$PSScriptRoot/../../")
$buildDir = "$drawpileDir/build-win-msvc"
if (!(Test-Path $buildDir)) {
    Write-Error "$buildDir does not exist. Make sure to run build-release.ps1 first."
    exit 1
}

$qtTranslationsDir = "$vcpkgDir/installed/$vcpkgTriplet/share/qt5/translations"
Write-Output "Using translations files from $qtTranslationsDir"

$outDir = "$PSScriptRoot/out"
New-Item -Path $outDir -ItemType Directory -Force
Remove-Item "$outDir/*" -Recurse -Force

# Copy binaries
Copy-Item -Path "$buildDir/bin/drawpile.exe" -Destination $outDir
Copy-Item -Path "$buildDir/bin/drawpile-srv.exe" -Destination $outDir

# Copy ressources
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
Compress-Archive -Path "$outDir/*" -DestinationPath "$outDir/../drawpile-win-msvc.zip" -CompressionLevel "Optimal" -Force

# Clean up
Remove-Item $outDir -Recurse -Force

Set-Location -Path $location
