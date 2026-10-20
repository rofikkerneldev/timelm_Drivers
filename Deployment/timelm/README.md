# Timelm first-boot provisioning

This ARM64 answer file automates the fresh image's specialize and oobeSystem
passes. It enables the built-in Administrator, configures blank-password
Administrator autologon, and creates a local Timelm administrator account so
the OOBE account phase is answered. Online account and wireless setup pages
are hidden. Locale is Indonesia, UI is en-US, keyboard is US, timezone is Jakarta.
Sleep, display timeout and hibernation are disabled during diagnosis.

Install these files in the offline phone image's `TimelmTools/OobeHelper` before
its first boot. The payload requires `timelm-device.marker` containing
`LG-V60-timelm-8509C01F`. Set the **offline phone SYSTEM** hive's
`Setup/UnattendFile` to `C:\TimelmTools\OobeHelper\Unattend.xml`. The deployed
phone Windows uses C:; do not edit the host PC's SYSTEM hive. Cache a matching
answer file in `Windows/Panther/Unattend/Unattend.xml` as well.

Place the built GPIO/SPMI/PMIC packages and their `testsigner.cer` certificates
under `TimelmTools/DriverPackages`. The helper imports the public certificates
during specialize. Test-signed kernel drivers also require TESTSIGNING in the
phone's own BCD. These files do not enable touchscreen or USB hardware.

Scripts refuse to run outside the marked ARM64 installed Windows or in WinPE.
They export setup, System, Code Integrity, boot configuration and PnP problem
logs to `C:\TimelmLogs\OobeHelper`. `Disable-AutoLogon.cmd` stops autologon later.

## Deployment validation on 2026-10-20

Windows 11 Pro image index 1, metadata 26300.9550, was applied with DISM Verify
to the V60 win partition. Driver packages from commit
`fc7e31658df8e33c5bc0b3414a5208397f673ffb` were installed with DISM Add-Driver.
All three SYS/CAT pairs matched the original build in DriverStore, and their
boot-start service paths referenced those packages. The fresh normal Windows
BCD default, test-signing flag and disabled recovery redirect were independently
checked. Desktop/OOBE completion and hardware operation still require a device
boot test.

On this PC, offline hive loading returned error 206 and DISM OS initialization
returned error 87 from the desktop-launched servicing process. Both succeeded
through a one-shot SYSTEM task. The temporary tasks were removed after servicing.

References: [Automate OOBE](https://learn.microsoft.com/en-us/windows-hardware/customize/desktop/automate-oobe),
[Administrator autologon](https://learn.microsoft.com/en-us/windows-hardware/customize/desktop/unattend/microsoft-windows-shell-setup-autologon-username),
[Answer-file discovery](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-setup-automation-overview),
[Test-signing](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option).
