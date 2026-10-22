LG V60 timelm - pemasangan Windows bersih

Unattend.xml dipasang sebelum boot pertama, bukan sesudah OOBE berjalan.
Target: Windows 11 Pro ARM64 dari ESD lokal, index 1, 26300.9550.

Konfigurasi:
- OOBE menggunakan jawaban otomatis; tidak mengandalkan klik Next.
- Akun Microsoft dan koneksi internet tidak diperlukan untuk akun lokal.
- Administrator bawaan aktif, tanpa password, masuk otomatis sampai 99 logon.
- Akun lokal Timelm (grup Administrators, tanpa password) disiapkan juga agar
  tahapan pembuatan akun OOBE sudah terjawab dan tersedia akun lokal tambahan.
- Wilayah Indonesia, UI Inggris, keyboard US, zona waktu Jakarta.
- Selama diagnosis, sleep dan timeout layar dimatikan agar layar idle tidak
  disalahartikan sebagai kegagalan boot; hibernasi juga dimatikan.
- Tidak menggunakan Audit Mode, SkipMachineOOBE, atau patch status setup.
- Helper hanya berjalan pada Windows ARM64 ponsel dengan marker, bukan WinPE.
- Log specialize dan desktop: C:\TimelmLogs\OobeHelper.
- Disable-AutoLogon.cmd tersedia untuk menghentikan login otomatis kemudian.

Setup baru tetap harus diuji boot di perangkat. Helper tidak memperbaiki
driver display, touchscreen, USB atau kegagalan kernel sebelum OOBE.

Referensi:
https://learn.microsoft.com/en-us/windows-hardware/customize/desktop/automate-oobe
https://learn.microsoft.com/en-us/windows-hardware/customize/desktop/unattend/microsoft-windows-shell-setup-autologon-username
