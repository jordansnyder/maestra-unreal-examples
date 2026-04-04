@echo off
cd /d "%~dp0"

echo ============================================
echo  MaestraUnrealClean - Package for Windows
echo ============================================
echo.

echo Cleaning previous build...
if exist "Saved\Cooked" rmdir /s /q "Saved\Cooked"
if exist "Saved\StagedBuilds" rmdir /s /q "Saved\StagedBuilds"
if exist "Windows" rmdir /s /q "Windows"

echo Packaging...
call "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" ^
  BuildCookRun ^
  -project=C:\Users\jorda\Dev\maestra-unreal-examples\UE5Aurora\MaestraUnrealClean.uproject ^
  -nop4 -utf8output -cook -build -stage -archive -package -pak -iostore -compressed -prereqs ^
  -platform=Win64 ^
  -clientconfig=Development ^
  -archivedirectory=C:\Users\jorda\Dev\maestra-unreal-examples\UE5Aurora ^
  -target=MaestraUnrealClean ^
  -unrealexe="C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if %ERRORLEVEL% EQU 0 (
  echo.
  echo Build succeeded! Output: Windows\MaestraUnrealClean.exe
) else (
  echo.
  echo Build FAILED with exit code %ERRORLEVEL%
)
pause
