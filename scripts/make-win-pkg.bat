@echo off
REM Create a deployment directory from which InnoSetup can build the installer package
REM Usage:
REM 1. Compile Drawpile (e.g. using Qt Creator)
REM 2. Create a packaging target directory
REM 3. Run this script
REM 4. Run drawpile.iss to build the installer

REM Prerequisites:
REM  %PATH% must include Qt and mingw binary directories
REM  %1 is the build directory
REM  %2 is the source directory
REM  %3 is the target directory

IF NOT EXIST "%1%"	 (
	ECHO "build directory %1 does not exist!"
	EXIT /b
)

IF NOT EXIST "%2%" (
	ECHO "source directory %2 does not exist!"
	EXIT /b
)

IF NOT EXIST "%3%" (
	ECHO "target directory %3 does not exist!"
	EXIT /b
)

REM Copy executables to the target directory
COPY "%1\bin\drawpile.exe" "%3\"
COPY "%1\bin\drawpile-srv.exe" "%3\"
COPY "%1\bin\dprec2txt.exe" "%3\"

windeployqt --release --compiler-runtime "%3\drawpile.exe"

REM Copy translations
MKDIR "%3\i18n"
MOVE "%3\*.qm" "%3\i18n\"
COPY "%1\src\client\*.qm" "%3\i18n\"

REM Copy extra files
MKDIR "%3\palettes"
COPY "%2\desktop\palettes\*" "%3\palettes\"

COPY "%2\README.md" "%3\Readme.txt"
COPY "%2\AUTHORS" "%3\Authors.txt"
COPY "%2\ChangeLog" "%3\ChangeLog.txt"
COPY "%2\COPYING" "%3\License.txt"
