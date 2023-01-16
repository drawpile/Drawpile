Scripts for building Drawpile with MSVC and vcpkg.

`vcpkg-ports-qt5.12/` contains qt5 ports from vcpkg commit `6d4606fb795667ef58da5367dee867d6a350fff5` (Last commit with Qt 5.12), `kf5archive`, `freetype` and `openssl` (Qt5.12 only seems to be compatible with openssl 1.1).  
Applied extra patches on `qt5-base` and fixed the source download url to use `https://download.qt.io/archive/`.  
**`prl_parser.patch` breaks static builds.** It modifies `Qt5CoreConfig.cmake` to backport Qt5.14's prl parser, but causes cmake to generate broken makefiles and ninja files. Without the patch, build fails because Qt's dependencies are not linked.

For `package.ps1`, an up to date version of `Compress-Archive` required. Old versions of `Compress-Archive` produce an invalid path separator.  
You can install the latest version with the following command (Must be run as Administrator).  
```powershell
Install-Module Microsoft.PowerShell.Archive -MinimumVersion 1.2.3.0 -Repository PSGallery -Force
```
