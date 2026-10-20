@echo off
setlocal
if /i not "%PROCESSOR_ARCHITECTURE%"=="ARM64" exit /b 90
if not exist "%SystemDrive%\TimelmTools\OobeHelper\timelm-device.marker" exit /b 91
reg query HKLM\SYSTEM\CurrentControlSet\Control\MiniNT >nul 2>&1
if not errorlevel 1 exit /b 92
set "LOG=%SystemDrive%\TimelmLogs\OobeHelper"
if not exist "%LOG%" mkdir "%LOG%"
(
  echo First desktop command reached: %date% %time%
  ver
  whoami /user
  net user Administrator
  reg query HKLM\SYSTEM\Setup
  tasklist /fi "IMAGENAME eq explorer.exe"
) >"%LOG%\first-desktop.txt" 2>&1
copy /y "%WINDIR%\Panther\setupact.log" "%LOG%\setupact.log" >nul 2>&1
copy /y "%WINDIR%\Panther\setuperr.log" "%LOG%\setuperr.log" >nul 2>&1
wevtutil epl System "%LOG%\System.evtx" /ow:true >"%LOG%\event-export.txt" 2>&1
wevtutil epl Microsoft-Windows-CodeIntegrity/Operational "%LOG%\CodeIntegrity.evtx" /ow:true >>"%LOG%\event-export.txt" 2>&1
pnputil /enum-devices /problem >"%LOG%\problem-devices.txt" 2>&1
pnputil /enum-drivers >"%LOG%\drivers.txt" 2>&1
if exist "%WINDIR%\System32\CiTool.exe" "%WINDIR%\System32\CiTool.exe" -lp -json >"%LOG%\CodeIntegrityPolicies.json" 2>&1
bcdedit /enum {current} >"%LOG%\current-boot.txt" 2>&1
if exist "%USERPROFILE%\Desktop" copy /y "%~dp0README.txt" "%USERPROFILE%\Desktop\Timelm-Readme.txt" >nul
exit /b 0
