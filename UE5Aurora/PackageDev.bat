@echo off
cd /d "%~dp0"

echo ============================================
echo  MaestraUnrealClean - Package for Windows
echo ============================================
echo.

REM echo Cleaning previous staged build...
REM if exist "Saved\StagedBuilds" rmdir /s /q "Saved\StagedBuilds"
REM if exist "Windows" rmdir /s /q "Windows"

echo Packaging...
call "D:\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" ^
  BuildCookRun ^
  -project=C:\Users\Jordan\Documents\GitHub\maestra-unreal-examples\UE5Aurora\MaestraUnrealClean.uproject ^
  -nop4 -utf8output -cook -build -stage -archive -package -pak -iostore -compressed -prereqs -legacyiterative ^
  -platform=Win64 ^
  -clientconfig=Development ^
  -archivedirectory=C:\Users\Jordan\Documents\GitHub\maestra-unreal-examples\UE5Aurora ^
  -target=MaestraUnrealClean ^
  -unrealexe="D:\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if %ERRORLEVEL% EQU 0 (
  echo.
  echo Build succeeded! Output: Windows\MaestraUnrealClean.exe
) else (
  echo.
  echo Build FAILED with exit code %ERRORLEVEL%
)
pause
