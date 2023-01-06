param ($vcpkgDir, $vcpkgTriplet = "x64-windows", $drawdanceDir = (Resolve-Path -Path $PSScriptRoot+"/../../../Drawdance"))

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

$vcpkg = "$vcpkgDir\vcpkg.exe"
# Check that vcpkg runs
try {
    & $vcpkg --version | Out-Null
}
catch {
    Write-Error "$vcpkg not running. Please specify the path to vcpkg.exe directory using the -vcpkgDir parameter."
    exit 1
}

# Check that cl is in PATH
try {
    & cl | Out-Null
}
catch {
    Write-Error "Cannot find cl. Make sure it is in PATH. (By running vsvarsall.bat)."
    exit 1
}

# Check if Drawdance seems right
if (!(Test-Path $drawdanceDir/CMakeLists.txt)) {
    Write-Error "Drawdance directory not found. Please specify the path to Drawdance directory using the -drawdanceDir parameter."
    exit 1
}

Write-Output "Using vcpkg from $vcpkgDir"
Write-Output "Using triplet $vcpkgTriplet"
Write-Output "Using drawdance $drawdanceDir"

# Install dependencies
$overlayPortsDir = (Resolve-Path -Path "$PSScriptRoot/vcpkg-ports-qt5.12")
& $vcpkg install qt5-base libpng libjpeg-turbo kf5archive curl --triplet=$vcpkgTriplet --host-triplet=$vcpkgTriplet "--overlay-ports=$overlayPortsDir" --clean-buildtrees-after-build --clean-packages-after-build # Drawdance deps
& $vcpkg install ecm qt5-base qt5-multimedia qt5-svg qt5-translations libsodium qtkeychain kf5archive --triplet=$vcpkgTriplet --host-triplet=$vcpkgTriplet "--overlay-ports=$overlayPortsDir" --clean-buildtrees-after-build --clean-packages-after-build # Drawpile deps

# Build Drawdance
Set-Location -Path $drawdanceDir
New-Item -Path "$drawdanceDir/build-win-msvc" -ItemType Directory -Force
Set-Location -Path "$drawdanceDir/build-win-msvc"
& cmake "-DCMAKE_TOOLCHAIN_FILE=$vcpkgDir/scripts/buildsystems/vcpkg.cmake" "-DVCPKG_TARGET_TRIPLET=$vcpkgTriplet" "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DBUILD_APPS=OFF -DUSE_GENERATORS=OFF -DUSE_CLANG_TIDY=OFF -DTHREAD_IMPL=QT -DXML_IMPL=QT -DZIP_IMPL=KARCHIVE -DLINK_WITH_LIBM=OFF -DCMAKE_BUILD_TYPE=Release -DUSE_ADDRESS_SANITIZER=OFF -G Ninja ../
& cmake --build .

# Build Drawpile
$drawpileDir = (Resolve-Path -Path "$PSScriptRoot/../../")
Set-Location -Path $drawpileDir
New-Item -Path "$drawpileDir/build-win-msvc" -ItemType Directory -Force
Set-Location -Path "$drawpileDir/build-win-msvc"
& cmake "-DDRAWDANCE_EXPORT_PATH=$drawdanceDir/build-win-msvc/Drawdance.cmake" "-DCMAKE_TOOLCHAIN_FILE=$vcpkgDir/scripts/buildsystems/vcpkg.cmake" "-DVCPKG_TARGET_TRIPLET=$vcpkgTriplet" "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DUSE_CLANG_TIDY=OFF -DUSE_ADDRESS_SANITIZER=OFF -DKIS_TABLET=ON -DCMAKE_BUILD_TYPE=Release -G Ninja ../
& cmake --build .

Write-Output "Build complete. Drawpile binaries are in $drawpileDir/build-win-msvc/bin"

Set-Location -Path $location
