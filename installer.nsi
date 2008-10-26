; by M.K.A. <wyrmchild@users.sourceforge.net>
; last modified: 26 Oct, 2008

;--------------------------------
; Basics

!define AppName "DrawPile"
!define Year "2006, 2007, 2008"
!define CopyrightHolders "callaa"
!define Website "http://drawpile.sourceforge.net/"
!define UpdateURL "http://drawpile.sourceforge.net/"

; Release version
!define VersionMajor 0
!define VersionMinor 6
!define BugFix 0
!define BugFixMinor 0

#!define ReleaseVersion "${VersionMajor}.${VersionMinor}.${BugFix}.${BugFixMinor}"
!define ReleaseVersion "${VersionMajor}.${VersionMinor}.${BugFix}"
#!define ReleaseVersion "SVN-r#"

; Component versions
!define Qt4Version "4.4.3" ; Qt4
!define MinGWVersion "3.15" ; MinGW Runtime

;--------------------------------

Name "${AppName} ${ReleaseVersion}"

; The file to write (installer exec)
OutFile "${AppName}-${ReleaseVersion}-installer.exe"

;--------------------------------
;   Install location

InstallDir $PROGRAMFILES\${AppName}

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)

InstallDirRegKey HKLM "Software\${AppName}" "install dir"

;--------------------------------
;   Installer compression

CRCCheck force

SetCompress auto
;SetCompressor /SOLID lzma
SetCompressor /FINAL /SOLID lzma
SetDatablockOptimize On
SetCompressorDictSize 16

LicenseData COPYING
;LicenseForceSelection checkbox "I accept"

; Save file dates
SetDateSave on

; Overwrite files if different
;SetOverwrite ifdiff

;--------------------------------
;   Installer look

XPStyle off
InstProgressFlags smooth colored

; Icon path\file.ico

InstallColors /windows

;--------------------------------
;   Installer information

LoadLanguageFile "${NSISDIR}\Contrib\Language files\English.nlf"

VIProductVersion ${VersionMajor}.${VersionMinor}.${BugFix}.0

VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" ${AppName}
VIAddVersionKey /LANG=${LANG_ENGLISH} "Comments" ${Website}
;VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" ""
;VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalTrademarks" ""
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "(c) ${Year} ${CopyrightHolders}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${AppName} installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" ${ReleaseVersion}

;--------------------------------
;   Installer pages

InstType "Full"

Page license
Page components
Page directory
Page instfiles

;UninstPage components
;UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------
;   Dummy

Section ""
  SetOutPath $INSTDIR
  
  WriteRegStr HKLM "Software\${AppName}" "path" "$INSTDIR"
  
  ; Uninstaller stuff.
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}" "DisplayName" "${AppName}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}" "NoRepair" 1
  
  ; the following apparently don't do anythng.
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}" "URLUpdateInfo" "${UpdateURL}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}" "URLInfoAbout" "${Website}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}" "HelpLink" "${Website}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}" "VersionMinor" "${VersionMinor}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}" "VersionMajor" "${VersionMajor}"
  
  ; write the uninstaller
  WriteUninstaller "uninstall.exe"
SectionEnd

;--------------------------------
;   Components

; SectioIn: 1 = full, 2 = minimal ?

; Client executable
Section "!Client" Client
  SectionIn 1
  File "bin\drawpile.exe"
SectionEnd ; end the section

; Server executable
Section "!Server" Server
  SectionIn 1
  File "bin\drawpile-srv.exe"
SectionEnd

SectionGroup "DLLs"

; MinGW runtime
; TODO - check if we can find MinGW install location?
Section "MinGW ${MinGWVersion} run-time" MinGW
  SectionIn 1
  File "bin\mingwm10.dll"
SectionEnd

; Qt4 DLLs
; TODO - split QtGUI to sub selection
; These need to be placed in lib folder manually
; TODO - check if we can find Qt install location?
Section "Qt ${Qt4Version} DLLs" Qt4
  SectionIn 1
  File "bin\QtCore4.dll"
  File "bin\QtGUI4.dll"
  File "bin\QtNetwork4.dll"
SectionEnd

SectionGroupEnd

SectionGroup "Translations"

; START LANGUAGES

; ENGLISH
Section "English [built-in]"
  SectionIn ro
SectionEnd

; FINNISH
;Section /o "Suomi (Finnish)"
;  SectionIn 1
;  
;  File "i18n/Suomi (Finnish).lang"
;SectionEnd

; SWEDISH
;Section /o "svenska (Swedish)"
;  SectionIn 1
;  File "i18n/svenska (Swedish).lang"
;SectionEnd

; CHINESE (simplified)
;Section /o "官话 (Simplified Chinese)" ; alternatively: 北方话
;  SectionIn 1
;  File "i18n/官话 (Simplified Chinese).lang"
;SectionEnd

; CHINESE (traditional)
;Section /o "官話 (Traditional Chinese)" ; alternatively: 北方話
;  SectionIn 1
;  File "i18n/官話 (Traditional Chinese).lang"
;SectionEnd

; JAPANESE
;Section /o "日本語 (Japanese)"
;  SectionIn 1
;  File "i18n/日本語 (Japanese).lang"
;SectionEnd

; ARABIC
;Section /o "اللغة العربية (Arabic)"
;  SectionIn 1
;  File "i18n/اللغة العربية (Arabic).lang"
;SectionEnd

; END LANGUAGES

SectionGroupEnd ; Translations

; START EXTRA FILES

SectionGroup "Documentation"

Section "License text"
  SectionIn ro
  File "COPYING"
SectionEnd

;Section "ReleaseNotes.txt"
;	SectionIn 1
;	File ReleaseNotes.txt
;SectionEnd

Section "ChangeLog"
	SectionIn 1
	File "ChangeLog"
SectionEnd

SectionGroupEnd ; Documentation

; END EXTRA FILES

; START-MENU
Section "Start Menu shortcuts"
  SectionIn 1
  
  CreateDirectory "$SMPROGRAMS\${AppName}"
  CreateShortCut "$SMPROGRAMS\${AppName}\${AppName} client.lnk" "$INSTDIR\drawpile.exe" "" "$INSTDIR\drawpile.exe" 0
  CreateShortCut "$SMPROGRAMS\${AppName}\${AppName} server.lnk" "$INSTDIR\drawpile-srv.exe" "" "$INSTDIR\drawpile-srv.exe" 0
  ;CreateShortCut "$SMPROGRAMS\${AppName}\ChangeLog.lnk" "$INSTDIR\ChangeLog"
  CreateShortCut "$SMPROGRAMS\${AppName}\Visit our website.lnk" "${Website}" "" "" 0
  CreateShortCut "$SMPROGRAMS\${AppName}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
SectionEnd

;-------------------
;   Uninstalling

Section "un."
  Delete /REBOOTOK "$INSTDIR\drawpile.exe"
  Delete /REBOOTOK "$INSTDIR\drawpile-srv.exe"
  Delete /REBOOTOK "$INSTDIR\QtCore4.dll"
  Delete /REBOOTOK "$INSTDIR\QtGUI4.dll"
  Delete /REBOOTOK "$INSTDIR\QtNetwork4.dll"
  Delete /REBOOTOK "$INSTDIR\mingwm10.dll"
  Delete /REBOOTOK "$INSTDIR\COPYING"
  Delete /REBOOTOK "$INSTDIR\ChangeLog"
  
  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\${AppName}\*.*"
  RMDir "$SMPROGRAMS\${AppName}"
  
  ; remove uninstaller
  Delete /REBOOTOK "$INSTDIR\uninstall.exe"
  
  ; remove installation directory
  RMDir /REBOOTOK "$INSTDIR"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppName}"
  DeleteRegKey HKLM "SOFTWARE\${AppName}"
  
  Call un.delUserSettings
SectionEnd

; DELETE USER SETTINGS func
Function un.delUserSettings
  MessageBox MB_YESNO "Remove user settings?" IDNO +4
  DeleteRegKey HKCU "SOFTWARE\${AppName}"
  DetailPrint "User settings removed!"
  Goto +2
  DetailPrint "User settings preserved!"
FunctionEnd

; TODO - ask if we're to delete anything in %AppData%

;SubSection "Uninstaller"
;	MessageBox MB_OK "Foo!"
;SubSectionEnd

;SubSection /e "Uninstaller" ssUninst
;  Section "Uninstaller" mbUninst
;    MessageBox MB_YESNO "Remove user settings?" IDNO +2
;    Call un.delUserSettings ; YES
;  SectionEnd
;SubSectionEnd
