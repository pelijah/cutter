@ECHO off
SETLOCAL ENABLEDELAYEDEXPANSION

IF "%VisualStudioVersion%" == "14.0" ( IF NOT DEFINED Platform SET "Platform=X86" )
FOR /F %%i IN ('powershell -c "\"%Platform%\".toLower()"') DO SET PLATFORM=%%i
powershell -c "if ('%PLATFORM%' -notin ('x86', 'x64')) {Exit 1}"
IF !ERRORLEVEL! NEQ 0 (
    ECHO Unknown platform: %PLATFORM%
    EXIT /B 1
)

SET "R2DIST=r2_dist_%PLATFORM%"
SET "BUILDDIR=build_%PLATFORM%"

ECHO Preparing directory
RMDIR /S /Q %BUILDDIR%
MKDIR %BUILDDIR%

ECHO Prepare translations
FOR %%i in (src\translations\*.ts) DO lrelease %%i

CD %BUILDDIR%

ECHO Building cutter
qmake %* ..\src\cutter.pro -config release
IF !ERRORLEVEL! NEQ 0 EXIT /B 1
nmake
IF !ERRORLEVEL! NEQ 0 EXIT /B 1

ECHO Deploying cutter
MKDIR cutter
COPY release\cutter.exe cutter\cutter.exe
XCOPY /S /I ..\%R2DIST%\radare2 cutter\radare2
COPY ..\%R2DIST%\*.dll cutter\
windeployqt cutter\cutter.exe
FOR %%i in (..\src\translations\*.qm) DO MOVE "%%~fi" cutter\translations
