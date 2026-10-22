@echo off
setlocal
if /i not "%PROCESSOR_ARCHITECTURE%"=="ARM64" exit /b 90
if not exist "%SystemDrive%\TimelmTools\OobeHelper\timelm-device.marker" exit /b 91
reg query HKLM\SYSTEM\CurrentControlSet\Control\MiniNT >nul 2>&1
if not errorlevel 1 exit /b 92
set "LOG=%SystemDrive%\TimelmLogs\OobeHelper"
if not exist "%LOG%" mkdir "%LOG%"
call :prepare >"%LOG%\specialize.txt" 2>&1
exit /b %errorlevel%
:prepare
echo Specialize reached: %date% %time%
net user Administrator /active:yes
if errorlevel 1 exit /b 1
for /r "%SystemDrive%\TimelmTools\DriverPackages" %%F in (testsigner.cer) do (
  if exist "%%F" (
    certutil -f -addstore Root "%%F"
    certutil -f -addstore TrustedPublisher "%%F"
  )
)
powercfg /hibernate off
powercfg /change standby-timeout-ac 0
powercfg /change standby-timeout-dc 0
powercfg /change monitor-timeout-ac 0
powercfg /change monitor-timeout-dc 0
echo Unattended OOBE will configure local accounts without a Microsoft account.
exit /b 0
