Unicode true

; Defined on command line.
;!define OUTFILE "slicer2-installer.exe"

; Defined on command line.
;!define EXE_SOURCES "slicer2"

!define ORG "ornl"
!define APPLICATION "slicer2"

!define HUMAN_ORG "ORNL"
!define HUMAN_APPLICATION "ORNL Slicer 2"

;!tempfile VER_TEMP
;!system 'echo "!define VERSION_MAJOR  $(jq -j .major  ../version.json)" >> ${VER_TEMP}'
;!system 'echo "!define VERSION_MINOR  $(jq -j .minor  ../version.json)" >> ${VER_TEMP}'
;!system 'echo "!define VERSION_PATCH  $(jq -j .patch  ../version.json)" >> ${VER_TEMP}'
;!system 'echo "!define VERSION_SUFFIX $(jq -j .suffix ../version.json)" >> ${VER_TEMP}'
;!include ${VER_TEMP}

; Defined on command line.
;!define VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}+${VERSION_SUFFIX}"
!system "echo ${APPLICATION} version: ${VERSION}"

!define ICON "../resources/icons/slicer-2_logo.ico"

; Info
Name "ORNL Slicer 2"
LicenseData "../LICENSE.md"
Icon "${ICON}"
UninstallIcon "${ICON}"
BrandingText "${VERSION}"
InstallDir "$PROGRAMFILES64\${ORG}\${APPLICATION}"
InstallDirRegKey HKLM "Software\${ORG}\${APPLICATION}" "Install_Dir"
RequestExecutionLevel admin
OutFile "${OUTFILE}"

; Pages
Page license
Page components
Page directory
Page instfiles

; Uninstaller Pages
UninstPage uninstConfirm
UninstPage instfiles

; Components
Section "${HUMAN_APPLICATION}"
    SectionIN RO

    WriteRegStr HKLM "Software\${ORG}\${APPLICATION}" "Install_Dir" "$INSTDIR"
    SetOutPath $INSTDIR

    File /r "../${EXE_SOURCES}/"
    File "${ICON}"

    ; Generate uninstaller
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "DisplayName"          "${HUMAN_APPLICATION}"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "DisplayIcon"          "$INSTDIR\slicer-2_logo.ico"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "Publisher"            "Oak Ridge National Lab"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "InstallLocation"      "$INSTDIR"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "DisplayVersion"       "${VERSION}"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "Contact"              "slicer@ornl.gov"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "HelpLink"             "https://github.com/ORNLSlicer/Slicer-2"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "URLInfoAbout"         "https://github.com/ORNLSlicer/Slicer-2"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "URLUpdateInfo"        "https://github.com/ORNLSlicer/Slicer-2/releases"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "Readme"               "$INSTDIR\share\doc\slicer2\slicer-2_user_guide.pdf"
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "Comments"             "An advanced object slicer by ORNL"

    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "UninstallString"      "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"

    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}" "NoRepair" 1

    WriteUninstaller "uninstall.exe"
SectionEnd

Section "Start Menu Shortcuts"
    SetShellVarContext all

    CreateDirectory "$SMPROGRAMS\${HUMAN_ORG}\${HUMAN_APPLICATION}"
    CreateShortcut  "$SMPROGRAMS\${HUMAN_ORG}\${HUMAN_APPLICATION}\Uninstall.lnk"            "$INSTDIR\uninstall.exe"          "" "$INSTDIR\slicer-2_logo.ico" 0
    CreateShortcut  "$SMPROGRAMS\${HUMAN_ORG}\${HUMAN_APPLICATION}\${HUMAN_APPLICATION}.lnk" "$INSTDIR\bin\${APPLICATION}.exe" "" "$INSTDIR\slicer-2_logo.ico" 0
SectionEnd

Section "Uninstall"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION}"
    DeleteRegKey HKLM "Software\${ORG}\${APPLICATION}"

    SetShellVarContext all

    RMDir /r "$INSTDIR"
    RMDir /r "$SMPROGRAMS\${HUMAN_ORG}\${HUMAN_APPLICATION}"
SectionEnd
