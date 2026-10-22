@echo off
setlocal
if /i not "%PROCESSOR_ARCHITECTURE%"=="ARM64" exit /b 90
if not exist "%SystemDrive%\TimelmTools\OobeHelper\timelm-device.marker" exit /b 91
reg query HKLM\SYSTEM\CurrentControlSet\Control\MiniNT >nul 2>&1
if not errorlevel 1 exit /b 92
net session >nul 2>&1
if errorlevel 1 (
 echo Jalankan sebagai Administrator di Windows ponsel.
 exit /b 5
)
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v AutoAdminLogon /t REG_SZ /d 0 /f
if errorlevel 1 exit /b 1
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v AutoLogonCount /t REG_DWORD /d 0 /f
exit /b %errorlevel%
