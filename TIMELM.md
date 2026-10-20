# LG V60 timelm baseline

Based on 3dcdec0bd59e3e77be92629b2ed9e095ae6e19e8 in this repository.
The GPIO change and PinConfig.h come from test_mu-timelm's timelm-gpio.patch
(SHA256 587309ce728b023fe2ca38bd815a8fd9d2cc809497e484f3cac5de020b5c040b).
It corrects pull configuration, interrupt edge programming and masking.
SPMI and PMIC transport code remains at the upstream baseline. The later touch
power writes and DMA/SMMU bypass experiments are not included in this build.

The timelm ACPI variant is copied from the current Mu timelm overlay. It retains
V60 UFS resources (0x01D84000 / 0x1C000, GSIV 297), TLMM (GSIV 240), the ordered
SPMI regions, and populated PMIC SID pairs 0/1, 2/3, 4/5, 8/9 and 10/11 read in
Recovery. It does not substitute Alioth's PMK8002 SID 6/7. It deliberately uses
the V60 firmware's existing USB and touchscreen bring-up status; adding Alioth
USB nodes would not establish clocks, role switching or regulators on V60.

ACPI AML belongs in the Mu firmware build. DISM installs only the ARM64 INF,
CAT and SYS packages; it does not replace firmware ACPI tables. This baseline
does not provide a working Windows touchscreen, USB host, PEP or display driver.

The timelm-baseline workflow builds three test-signed KMDF 1.33 packages and
exports their public signing certificates. Windows test-signing and any active
code-integrity policies must permit these packages. Successful compilation or
DISM staging alone is not hardware validation.
