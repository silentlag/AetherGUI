#pragma once
#include <string>
#include <vector>
#include <sstream>


namespace EmbeddedConfig {


inline std::vector<std::string> GetAllCommands() {
	const char* configData = R"CFG(
LogDirect true

# === tablet.cfg ===

# Wacom CTL-470 (Wacom drivers installed)
Tablet 0x056a 0x00dd 0xFF00 0x000A
Name "Wacom CTL-470 (Wacom drivers installed)"
ReportId 0x02
ReportLength 11
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.2
Height 92.0
Type WacomDrivers

# Wacom CTL-470
Tablet 0x056a 0x00dd 0x0D 0x01
Name "Wacom CTL-470"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.2
Height 92.0
InitFeature 0x02 0x02

# Wacom CTH-470
Tablet 0x056a 0x00de 0x0D 0x01
Name "Wacom CTH-470"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.2
Height 92.0
InitFeature 0x02 0x02

# Wacom CTH-670
Tablet 0x056a 0x00df 0x0D 0x01
Name "Wacom CTH-670"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 21648
MaxY 13700
MaxPressure 1023
Width 216.48
Height 137.0
InitFeature 0x02 0x02

# Wacom CTL-471 (Wacom drivers installed)
Tablet 0x056a 0x0300 0xFF00 0x000A
Name "Wacom CTL-471 (Wacom drivers installed)"
ReportLength 11
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 1023
Width 152.00
Height 95.00
Type WacomDrivers

# Wacom CTL-471
Tablet 0x056a 0x0300 0xFF0D 0x01
Name "Wacom CTL-471"
ReportLength 10
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 1023
Width 152.00
Height 95.00
InitFeature 0x02 0x02

# Wacom CTL-671
Tablet 0x056a 0x0301 0xFF0D 0x01
Name "Wacom CTL-671"
ReportLength 10
DetectMask 0x40
MaxX 21648
MaxY 13530
MaxPressure 1023
Width 216.48
Height 135.30
InitFeature 0x02 0x02

# Wacom CTL-472 (Wacom drivers installed)
Tablet 0x056a 0x037a 0xFF00 0x000A
Name "Wacom CTL-472 (Wacom drivers installed)"
ReportId 0x02
ReportLength 11
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 2047
Width 152.0
Height 95.0
Type WacomDrivers

# Wacom CTL-472
Tablet 0x056a 0x037a 0xFF0D 0x0001
Name "Wacom CTL-472"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 2047
Width 152.0
Height 95.0
InitFeature 0x02 0x02

# Wacom CTL-672
Tablet 0x056a 0x037b 0xFF0D 0x0001
Name "Wacom CTL-672"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 21600
MaxY 13500
MaxPressure 2047
Width 216.00
Height 135.00
InitFeature 0x02 0x02

# Wacom CTL-480 (native match)
Tablet 0x056a 0x030e 0x0000 0x0000 10
Name "Wacom CTL-480"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 1023
Width 152.0
Height 95.0
InitFeature 0x02 0x02

# Wacom CTL-480 (Wacom drivers installed)
Tablet 0x056a 0x030e 0x0000 0x0000 11
Tablet 0x056a 0x030e 0xFF00 0x000A
Name "Wacom CTL-480 (Wacom drivers installed)"
ReportId 0x02
ReportLength 11
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 1023
Width 152.0
Height 95.0
Type WacomDrivers

# Wacom CTL-480
Tablet 0x056a 0x030e 0xFF0D 0x0001
Name "Wacom CTL-480"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 1023
Width 152.0
Height 95.0
InitFeature 0x02 0x02

# Wacom CTH-480 (native match)
Tablet 0x056a 0x0302 0x0000 0x0000 10
Name "Wacom CTH-480"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 1023
Width 152.0
Height 95.0
InitFeature 0x02 0x02

# Wacom CTH-480 (Wacom drivers installed)
Tablet 0x056a 0x0302 0x0000 0x0000 11
Tablet 0x056a 0x0302 0xFF00 0x000A
Name "Wacom CTH-480 (Wacom drivers installed)"
ReportId 0x02
ReportLength 11
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 1023
Width 152.0
Height 95.0
Type WacomDrivers

# Wacom CTH-480
Tablet 0x056a 0x0302 0xFF0D 0x0001
Name "Wacom CTH-480"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 1023
Width 152.0
Height 95.0
InitFeature 0x02 0x02

# Wacom CTL-680
Tablet 0x056a 0x0323 0xFF0D 0x0001
Name "Wacom CTL-680"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 21600
MaxY 13500
MaxPressure 1023
Width 216.0
Height 135.0
InitFeature 0x02 0x02

# Wacom CTH-680
Tablet 0x056a 0x0303 0xFF0D 0x0001
Name "Wacom CTH-680"
ReportId 0x02
ReportLength 10
DetectMask 0x40
MaxX 21600
MaxY 13500
MaxPressure 1023
Width 216.0
Height 135.0
InitFeature 0x02 0x02

# Wacom CTL-490
Tablet 0x056a 0x033b 0xFF0D 0x0001
Name "Wacom CTL-490"
ReportId 0x10
ReportLength 10
DetectMask 0xA0
MaxX 15200
MaxY 9500
MaxPressure 2047
KeepTipDown 2 packets
Width 152.0
Height 95.0
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom CTH-490
Tablet 0x056a 0x033c 0xFF0D 0x0001
Name "Wacom CTH-490"
ReportId 0x10
ReportLength 10
DetectMask 0xA0
MaxX 15200
MaxY 9500
MaxPressure 2047
KeepTipDown 2 packets
Width 152.0
Height 95.0
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom CTL-4100
Tablet 0x056a 0x0376 0xFF0D 0x0001
Tablet 0x056a 0x0374 0xFF0D 0x0001
Name "Wacom CTL-4100"
ReportId 0x10
ReportLength 192
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152.0
Height 95.0
Type Wacom4100

# Wacom CTL-4100 (Wacom drivers installed)
Tablet 0x056a 0x0376 0xFF00 0x000A
Tablet 0x056a 0x0374 0xFF00 0x000A
Name "Wacom CTL-4100 (Wacom drivers installed)"
ReportId 0x10
ReportLength 193
DetectMask 0x40
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152.0
Height 95.0
Type Wacom4100

)CFG";

	const char* configDataWacomExtended = R"CFG(
# Wacom IntuosV2 family
# These entries use exact input report lengths so 192-byte native reports
# do not get mixed with 193-byte Wacom-driver reports.

# Wacom CTL-4100 (native match)
Tablet 0x056A 0x0374 0x0000 0x0000 192
Name "Wacom CTL-4100 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152.000
Height 95.000
Type WacomIntuosV2

# Wacom CTL-4100 (Wacom drivers installed, native match)
Tablet 0x056A 0x0374 0x0000 0x0000 193
Name "Wacom CTL-4100 (Wacom drivers installed, native match)"
ReportLength 193
ReportOffset 1
DetectMask 0x20
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152.000
Height 95.000
Type WacomIntuosV2

# Wacom CTL-4100WL (native match)
Tablet 0x056A 0x0376 0x0000 0x0000 64
Tablet 0x056A 0x0376 0x0000 0x0000 192
Tablet 0x056A 0x0377 0x0000 0x0000 361
Tablet 0x056A 0x03C5 0x0000 0x0000 192
Name "Wacom CTL-4100WL (native match)"
ReportLength 192
DetectMask 0x20
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152.000
Height 95.000
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom CTL-4100WL (Wacom drivers installed, native match)
Tablet 0x056A 0x0376 0x0000 0x0000 193
Tablet 0x056A 0x0377 0x0000 0x0000 193
Tablet 0x056A 0x03C5 0x0000 0x0000 193
Name "Wacom CTL-4100WL (Wacom drivers installed, native match)"
ReportLength 193
ReportOffset 1
DetectMask 0x20
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152.000
Height 95.000
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom CTL-6100 (native match)
Tablet 0x056A 0x0375 0x0000 0x0000 192
Name "Wacom CTL-6100 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 21600
MaxY 13500
MaxPressure 4095
Width 216.000
Height 135.000
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom CTL-6100WL (native match)
Tablet 0x056A 0x0378 0x0000 0x0000 192
Tablet 0x056A 0x03C7 0x0000 0x0000 192
Name "Wacom CTL-6100WL (native match)"
ReportLength 192
DetectMask 0x20
MaxX 21600
MaxY 13500
MaxPressure 4095
Width 216.000
Height 135.000
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom DTC-121 (native match)
Tablet 0x056A 0x03CE 0x0000 0x0000 192
Name "Wacom DTC-121 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 25632
MaxY 14418
MaxPressure 4095
Width 256.320
Height 144.180
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom DTC-133 (native match)
Tablet 0x056A 0x03A6 0x0000 0x0000 192
Name "Wacom DTC-133 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 29434
MaxY 16556
MaxPressure 4095
Width 294.340
Height 165.560
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom DTC-133 (Wacom drivers installed, native match)
Tablet 0x056A 0x03A6 0x0000 0x0000 193
Name "Wacom DTC-133 (Wacom drivers installed, native match)"
ReportLength 193
ReportOffset 1
DetectMask 0x20
MaxX 29434
MaxY 16556
MaxPressure 4095
Width 294.340
Height 165.560
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom DTH-1320 (native match)
Tablet 0x056A 0x034F 0x0000 0x0000 192
Name "Wacom DTH-1320 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 59552
MaxY 33848
MaxPressure 8191
Width 297.760
Height 169.240
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom Cintiq 16 DTK-1660 (native match)
Tablet 0x056A 0x0390 0x0000 0x0000 192
Tablet 0x056A 0x03AE 0x0000 0x0000 192
Name "Wacom Cintiq 16 DTK-1660 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 69632
MaxY 39518
MaxPressure 8191
Width 348.160
Height 197.590
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom Cintiq Pro 22 DTH-227 (native match)
Tablet 0x056A 0x03D0 0x0000 0x0000 192
Name "Wacom Cintiq Pro 22 DTH-227 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 96012
MaxY 54356
MaxPressure 8191
Width 480.060
Height 271.780
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom Cintiq Pro 27 DTH-271 (native match)
Tablet 0x056A 0x03C0 0x0000 0x0000 192
Name "Wacom Cintiq Pro 27 DTH-271 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 120032
MaxY 67868
MaxPressure 8191
Width 600.160
Height 339.340
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom PTH-460 (native match)
Tablet 0x056A 0x0392 0x0000 0x0000 192
Tablet 0x056A 0x03DC 0x0000 0x0000 192
Name "Wacom PTH-460 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 31920
MaxY 19950
MaxPressure 8191
Width 159.600
Height 99.750
InitFeature 0x02 0x02
Type WacomIntuosV2

# Wacom PTH-460 (Wacom drivers installed, native match)
Tablet 0x056A 0x0392 0x0000 0x0000 193
Name "Wacom PTH-460 (Wacom drivers installed, native match)"
ReportLength 193
ReportOffset 1
DetectMask 0x20
MaxX 31920
MaxY 19950
MaxPressure 8191
Width 159.600
Height 99.750
Type WacomIntuosV2

# Wacom PTH-660 (native match)
Tablet 0x056A 0x0357 0x0000 0x0000 192
Name "Wacom PTH-660 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 44800
MaxY 29600
MaxPressure 8191
Width 224.000
Height 148.000
Type WacomIntuosV2

# Wacom PTH-860 (native match)
Tablet 0x056A 0x0358 0x0000 0x0000 192
Name "Wacom PTH-860 (native match)"
ReportLength 192
DetectMask 0x20
MaxX 62200
MaxY 43200
MaxPressure 8191
Width 311.000
Height 216.000
Type WacomIntuosV2

# Wacom PTH-860 (Wacom drivers installed, native match)
Tablet 0x056A 0x0358 0x0000 0x0000 193
Name "Wacom PTH-860 (Wacom drivers installed, native match)"
ReportLength 193
ReportOffset 1
DetectMask 0x20
MaxX 62200
MaxY 43200
MaxPressure 8191
Width 311.000
Height 216.000
Type WacomIntuosV2

# Wacom PTH-451
Tablet 0x056a 0x0314 0xFF0D 0x01
Name "Wacom PTH-451"
ReportLength 10
DetectMask 0xE0
MaxX 31496
MaxY 19685
MaxPressure 2047
Width 157.0
Height 98.0
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom PTH-850
Tablet 0x056A 0x0028 0x00D 0x0001
Name "Wacom PTH-850"
ReportLength 10
DetectMask 0xE0
MaxX 65024
MaxY 40640
MaxPressure 2047
Width 325.120
Height 203.200
InitFeature 0x02 0x02
Type WacomIntuos

)CFG";

	const char* configDataXpPenExtended1 = R"CFG(
# XP-Pen G430
Tablet 0x28BD 0x0075 0xFF0A 0x0001
Name "XP-Pen G430"
ReportId 0x02
ReportLength 8
DetectMask 0x80
MaxX 45720
MaxY 29210
MaxPressure 2047
Width 102.4
Height 76.2
InitReport 0x02 0xB0 0x02 0x00 0x00 0x00 0x00 0x00

# XP-Pen G540 Pro
Tablet 0x28BD 0x0061 0xFF0A 0x0001
Name "XP-Pen G540 Pro"
ReportId 0x02
ReportLength 8
DetectMask 0x80
MaxX 45720
MaxY 29210
MaxPressure 8191
Width 147.0
Height 101.6
InitReport 0x02 0xB0 0x02 0x00 0x00 0x00 0x00 0x00

# XP-Pen G640
Tablet 0x28BD 0x0094 0xFF0A 0x0001
Name "XP-Pen G640"
ReportId 0x02
ReportLength 8
DetectMask 0x80
MaxX 32000
MaxY 20000
MaxPressure 8191
Width 160.0
Height 100.0
InitReport 0x02 0xB0 0x02 0x00 0x00 0x00 0x00 0x00

# XP-Pen Deco 01
Tablet 0x28BD 0x0042 0xFF0A 0x0001
Name "XP-Pen Deco 01"
ReportId 0x02
ReportLength 8
DetectMask 0x80
MaxX 25400
MaxY 15875
MaxPressure 8191
Width 254.0
Height 158.75
InitReport 0x02 0xB0 0x02 0x00 0x00 0x00 0x00 0x00

# XP-Pen Artist 10 (2nd Gen)
Tablet 0x28BD 0x094D 0x0000 0x0000 12
Name "XP-Pen Artist 10 (2nd Gen)"
ReportLength 12
MaxX 44902
MaxY 25339
MaxPressure 8191
Width 224.510
Height 126.695
InitReport 0x02 0xB0 0x04
Type XPPenOffsetPressure

# XP-Pen Artist 10S
Tablet 0x5543 0x004A 0x0000 0x0000 8
Name "XP-Pen Artist 10S"
ReportLength 8
MaxX 21696
MaxY 13560
MaxPressure 2047
Width 216.960
Height 135.600
InitString 123
Type UCLogicV1

# XP-Pen Artist 12 (2nd Gen)
Tablet 0x28BD 0x094A 0x0000 0x0000 12
Name "XP-Pen Artist 12 (2nd Gen)"
ReportLength 12
MaxX 52638
MaxY 29616
MaxPressure 8191
Width 263.190
Height 148.080
InitReport 0x02 0xB0 0x04
Type XPPenOffsetPressure

# XP-Pen Artist 12 Pro
Tablet 0x28BD 0x091F 0x0000 0x0000 12
Name "XP-Pen Artist 12 Pro"
ReportLength 12
MaxX 25634
MaxY 14415
MaxPressure 8191
Width 256.340
Height 144.150
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist 12
Tablet 0x28BD 0x080A 0x0000 0x0000 10
Name "XP-Pen Artist 12"
ReportLength 10
MaxX 25632
MaxY 14418
MaxPressure 8191
Width 256.320
Height 144.180
InitReport 0x02 0xB0 0x04
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Artist 13 (2nd Gen)
Tablet 0x28BD 0x094E 0x0000 0x0000 12
Name "XP-Pen Artist 13 (2nd Gen)"
ReportLength 12
MaxX 58760
MaxY 33040
MaxPressure 8191
Width 293.800
Height 165.200
InitReport 0x02 0xB0 0x04
Type XPPenOffsetPressure

# XP-Pen Artist 13.3 Pro V2
Tablet 0x28BD 0x096E 0x0000 0x0000 14
Name "XP-Pen Artist 13.3 Pro V2"
ReportLength 14
MaxX 58760
MaxY 33045
MaxPressure 16383
Width 293.800
Height 165.225
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Artist 13.3 Pro
Tablet 0x28BD 0x092B 0x0000 0x0000 12
Name "XP-Pen Artist 13.3 Pro"
ReportLength 12
MaxX 29362
MaxY 16510
MaxPressure 8191
Width 293.620
Height 165.100
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist 13.3
Tablet 0x28BD 0x000B 0x0000 0x0000 8
Name "XP-Pen Artist 13.3"
ReportLength 8
MaxX 29376
MaxY 16524
MaxPressure 8191
Width 293.760
Height 165.240
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Artist 15.6 Pro V2
Tablet 0x28BD 0x096F 0x0000 0x0000 14
Name "XP-Pen Artist 15.6 Pro V2"
ReportLength 14
MaxX 68199
MaxY 38399
MaxPressure 16383
Width 340.995
Height 191.995
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Artist 15.6 Pro
Tablet 0x28BD 0x090D 0x0000 0x0000 10
Name "XP-Pen Artist 15.6 Pro"
ReportLength 10
MaxX 34416
MaxY 19359
MaxPressure 8191
Width 344.160
Height 193.590
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist 15.6 (1)
Tablet 0x28BD 0x000C 0x0000 0x0000 8
Name "XP-Pen Artist 15.6"
ReportLength 8
MaxX 34419
MaxY 19461
MaxPressure 8191
Width 344.190
Height 194.610
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Artist 15.6 (2)
Tablet 0x28BD 0x091A 0x0000 0x0000 10
Name "XP-Pen Artist 15.6"
ReportLength 10
MaxX 34419
MaxY 19461
MaxPressure 8191
Width 344.190
Height 194.610
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist 16 (2nd Gen)
Tablet 0x28BD 0x094C 0x0000 0x0000 12
Name "XP-Pen Artist 16 (2nd Gen)"
ReportLength 12
MaxX 68198
MaxY 38362
MaxPressure 8191
Width 340.990
Height 191.810
InitReport 0x02 0xB0 0x04
Type XPPenOffsetPressure

# XP-Pen Artist 16 Pro
Tablet 0x28BD 0x094B 0x0000 0x0000 12
Name "XP-Pen Artist 16 Pro"
ReportLength 12
MaxX 68214
MaxY 38359
MaxPressure 8191
Width 341.070
Height 191.795
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist 16
Tablet 0x5543 0x004D 0x0000 0x0000 8
Name "XP-Pen Artist 16"
ReportLength 8
MaxX 34416
MaxY 19359
MaxPressure 2047
Width 344.160
Height 193.590
InitString 100 123
Type XPPenOffsetAux

# XP-Pen Artist 22 (2nd Gen)
Tablet 0x28BD 0x092F 0x0000 0x0000 10
Name "XP-Pen Artist 22 (2nd Gen)"
ReportLength 10
MaxX 47664
MaxY 26778
MaxPressure 8191
Width 476.640
Height 267.780
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist 22HD
Tablet 0x5543 0x0047 0x0000 0x0000 8
Name "XP-Pen Artist 22HD"
ReportLength 8
MaxX 38161
MaxY 21458
MaxPressure 2047
Width 477.012
Height 268.225
InitString 100
Type XPPen

# XP-Pen Artist 22R Pro
Tablet 0x28BD 0x091B 0x0000 0x0000 10
Name "XP-Pen Artist 22R Pro"
ReportLength 10
MaxX 47664
MaxY 26778
MaxPressure 8191
Width 476.640
Height 267.780
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist 24 Pro
Tablet 0x28BD 0x092D 0x0000 0x0000 12
Name "XP-Pen Artist 24 Pro"
ReportLength 12
MaxX 105370
MaxY 59270
MaxPressure 8191
Width 526.850
Height 296.350
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist 24
Tablet 0x28BD 0x093A 0x0000 0x0000 12
Name "XP-Pen Artist 24"
ReportLength 12
MaxX 105370
MaxY 59270
MaxPressure 8191
Width 526.850
Height 296.350
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist Pro 14 (Gen2)
Tablet 0x28BD 0x095A 0x0000 0x0000 14
Name "XP-Pen Artist Pro 14 (Gen2)"
ReportLength 14
MaxX 60320
MaxY 37698
MaxPressure 16383
Width 301.600
Height 188.490
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Artist Pro 16 (Gen2)
Tablet 0x28BD 0x095B 0x0000 0x0000 14
Name "XP-Pen Artist Pro 16 (Gen2)"
ReportLength 14
MaxX 68920
MaxY 43078
MaxPressure 16383
Width 344.600
Height 215.390
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Artist Pro 16TP
Tablet 0x28BD 0x092E 0x0000 0x0000 12
Name "XP-Pen Artist Pro 16TP"
ReportLength 12
MaxX 69123
MaxY 38877
MaxPressure 8191
Width 345.615
Height 194.385
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Artist Pro 19 (Gen2)
Tablet 0x28BD 0x096A 0x0000 0x0000 14
Name "XP-Pen Artist Pro 19 (Gen2)"
ReportLength 14
MaxX 81793
MaxY 46009
MaxPressure 16383
Width 408.965
Height 230.045
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Artist Pro 22 (Gen2)
Tablet 0x28BD 0x096B 0x0000 0x0000 14
Name "XP-Pen Artist Pro 22 (Gen2)"
ReportLength 14
MaxX 95213
MaxY 53557
MaxPressure 16383
Width 476.065
Height 267.785
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Artist Pro 24 (Gen2)
Tablet 0x28BD 0x095C 0x0000 0x0000 14
Name "XP-Pen Artist Pro 24 (Gen2)"
ReportLength 14
MaxX 105216
MaxY 59182
MaxPressure 16383
Width 526.080
Height 295.910
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Deco Fun L (CT1060)
Tablet 0x28BD 0x0932 0x0000 0x0000 12
Name "XP-Pen Deco Fun L (CT1060)"
ReportLength 12
MaxX 50901
MaxY 31861
MaxPressure 8191
Width 254.505
Height 159.305
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco Fun XS (CT430)
Tablet 0x28BD 0x0930 0x0000 0x0000 12
Name "XP-Pen Deco Fun XS (CT430)"
ReportLength 12
MaxX 24384
MaxY 15240
MaxPressure 8191
Width 121.920
Height 76.200
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco Fun S (CT640)
Tablet 0x28BD 0x0931 0x0000 0x0000 12
Name "XP-Pen Deco Fun S (CT640)"
ReportLength 12
MaxX 32000
MaxY 20320
MaxPressure 8191
Width 160.000
Height 101.600
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco 01 V2 (Variant 2)
Tablet 0x28BD 0x0905 0x0000 0x0000 12 4 "(UG902_BPG1002|UG901_BPU1002)" 5 "^202\d-\d{2}-\d{2}"
Tablet 0x28BD 0x0902 0x0000 0x0000 12 4 "UG902_BPG1002" 5 "^202\d-\d{2}-\d{2}"
Name "XP-Pen Deco 01 V2 (Variant 2)"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254.000
Height 158.750
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco 01 V2
Tablet 0x28BD 0x0905 0x0000 0x0000 12 4 "UG901_BPU1002" 5 "^(?!202\d-\d{2}-\d{2})"
Tablet 0x28BD 0x0902 0x0000 0x0000 10 4 "UG901_BPU1002"
Tablet 0x28BD 0x0905 0x0000 0x0000 12 4 "UG902_BPG1002" 5 "\d{2}-\d{2}-\d{2}_realse001"
Name "XP-Pen Deco 01 V2"
ReportLength 12
MaxX 25400
MaxY 15875
MaxPressure 8191
Width 254.000
Height 158.750
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco 01 V3 (Variant 2)
Tablet 0x28BD 0x0947 0x0000 0x0000 12
Name "XP-Pen Deco 01 V3 (Variant 2)"
ReportLength 12
MaxX 51196
MaxY 31826
MaxPressure 16383
Width 255.980
Height 159.130
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco 01 V3
Tablet 0x28BD 0x0947 0x0000 0x0000 12
Name "XP-Pen Deco 01 V3"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 16383
Width 254.000
Height 158.750
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco 02
Tablet 0x28BD 0x0803 0x0000 0x0000 10
Name "XP-Pen Deco 02"
ReportLength 10
MaxX 22352
MaxY 13970
MaxPressure 8191
Width 254.000
Height 142.875
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Deco 03
Tablet 0x28BD 0x0096 0x0000 0x0000 8
Name "XP-Pen Deco 03"
ReportLength 8
MaxX 50800
MaxY 28575
MaxPressure 8191
Width 254.000
Height 142.875
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Deco 640 (IT640)
Tablet 0x28BD 0x2904 0x0000 0x0000 12
Name "XP-Pen Deco 640 (IT640)"
ReportLength 12
MaxX 31998
MaxY 17998
MaxPressure 16383
Width 159.990
Height 89.990
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco L
Tablet 0x28BD 0x0935 0x0000 0x0000 10
Name "XP-Pen Deco L"
ReportLength 10
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254.000
Height 152.400
InitReport 0x02 0xB0 0x04
Type XPPenOffsetPressure

# XP-Pen Deco LW
Tablet 0x28BD 0x0935 0x0000 0x0000 12
Name "XP-Pen Deco LW"
ReportLength 12
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254.000
Height 152.400
InitReport 0x02 0xB0 0x04
Type XPPenOffsetPressure

# XP-Pen Deco M
Tablet 0x28BD 0x0936 0x0000 0x0000 12
Name "XP-Pen Deco M"
ReportLength 12
MaxX 40640
MaxY 25400
MaxPressure 8191
Width 203.200
Height 127.000
InitReport 0x02 0xB0 0x04
Type XPPenOffsetPressure

# XP-Pen Deco mini4
Tablet 0x28BD 0x0929 0x0000 0x0000 12
Name "XP-Pen Deco mini4"
ReportLength 12
MaxX 20320
MaxY 15240
MaxPressure 8191
Width 101.600
Height 76.200
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco mini7 V2
Tablet 0x28BD 0x0948 0x0000 0x0000 12
Name "XP-Pen Deco mini7 V2"
ReportLength 12
MaxX 35595
MaxY 22219
MaxPressure 16383
Width 177.975
Height 111.095
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco mini7
Tablet 0x28BD 0x0928 0x0000 0x0000 12
Name "XP-Pen Deco mini7"
ReportLength 12
MaxX 35560
MaxY 22219
MaxPressure 8191
Width 177.800
Height 111.095
InitReport 0x02 0xB0 0x04
Type XPPen

)CFG";

	const char* configDataXpPenExtended2 = R"CFG(
# XP-Pen Deco mini7W V2
Tablet 0x28BD 0x0949 0x0000 0x0000 12
Name "XP-Pen Deco mini7W V2"
ReportLength 12
MaxX 35595
MaxY 22219
MaxPressure 16383
Width 177.975
Height 111.095
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco MW
Tablet 0x28BD 0x0936 0x0000 0x0000 14
Name "XP-Pen Deco MW"
ReportLength 14
MaxX 40640
MaxY 25400
MaxPressure 8191
Width 203.200
Height 127.000
InitReport 0x02 0xB0 0x04
Type XPPenOffsetPressure

# XP-Pen Deco Pro LW Gen2
Tablet 0x28BD 0x0943 0x0000 0x0000 14
Name "XP-Pen Deco Pro LW Gen2"
ReportLength 14
MaxX 55798
MaxY 34798
MaxPressure 16383
Width 278.990
Height 173.990
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Deco Pro Medium
Tablet 0x28BD 0x0904 0x0000 0x0000 10
Name "XP-Pen Deco Pro Medium"
ReportLength 10
MaxX 55798
MaxY 31399
MaxPressure 8191
Width 278.990
Height 156.995
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco Pro MW Gen2
Tablet 0x28BD 0x0942 0x0000 0x0000 14
Name "XP-Pen Deco Pro MW Gen2"
ReportLength 14
MaxX 45801
MaxY 28600
MaxPressure 16383
Width 229.005
Height 143.000
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Deco Pro MW
Tablet 0x28BD 0x0934 0x0000 0x0000 12
Name "XP-Pen Deco Pro MW"
ReportLength 12
MaxX 55798
MaxY 31399
MaxPressure 8191
Width 278.990
Height 156.995
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco Pro Small
Tablet 0x28BD 0x0909 0x0000 0x0000 10
Name "XP-Pen Deco Pro Small"
ReportLength 10
MaxX 46024
MaxY 25908
MaxPressure 8191
Width 230.120
Height 129.540
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco Pro SW
Tablet 0x28BD 0x0933 0x0000 0x0000 12
Name "XP-Pen Deco Pro SW"
ReportLength 12
MaxX 46024
MaxY 25908
MaxPressure 8191
Width 230.120
Height 129.540
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Deco Pro XLW Gen2
Tablet 0x28BD 0x0944 0x0000 0x0000 14
Name "XP-Pen Deco Pro XLW Gen2"
ReportLength 14
MaxX 76200
MaxY 45801
MaxPressure 16383
Width 381.000
Height 229.005
InitReport 0x02 0xB0 0x04
Type XPPenGen2

# XP-Pen Innovator 16
Tablet 0x28BD 0x092C 0x0000 0x0000 12
Name "XP-Pen Innovator 16"
ReportLength 12
MaxX 68783
MaxY 38709
MaxPressure 8191
Width 343.915
Height 193.545
InitReport 0x02 0xB1 0x04
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star 02
Tablet 0x5543 0x0050 0x0000 0x0000 8
Name "XP-Pen Star 02"
ReportLength 8
MaxX 32000
MaxY 20000
MaxPressure 2047
Width 203.200
Height 127.000
InitString 100 123
Type UCLogic

# XP-Pen Star 03 Pro
Tablet 0x28BD 0x0077 0x0000 0x0000 8
Name "XP-Pen Star 03 Pro"
ReportLength 8
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254.000
Height 152.400
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Star 03 V2
Tablet 0x28BD 0x0907 0x0000 0x0000 12
Name "XP-Pen Star 03 V2"
ReportLength 12
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254.000
Height 152.400
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star 03
Tablet 0x28BD 0x0907 0x0000 0x0000 10
Tablet 0x28BD 0x0907 0x0000 0x0000 12
Name "XP-Pen Star 03"
ReportLength 10
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254.000
Height 152.400
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star 05 V3
Tablet 0x28BD 0x0071 0x0000 0x0000 8
Name "XP-Pen Star 05 V3"
ReportLength 8
MaxX 40640
MaxY 25400
MaxPressure 8191
Width 203.200
Height 127.000
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Star 06
Tablet 0x28BD 0x0078 0x0000 0x0000 8
Name "XP-Pen Star 06"
ReportLength 8
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254.000
Height 152.400
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Star 06C
Tablet 0x28BD 0x0062 0x0000 0x0000 8
Name "XP-Pen Star 06C"
ReportLength 8
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254.000
Height 152.400
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Star G430 (1)
Tablet 0x28BD 0x0075 0x0000 0x0000 8 2 "TABLET G3 4x3"
Name "XP-Pen Star G430"
ReportLength 8
MaxX 45720
MaxY 29210
MaxPressure 2047
Width 101.600
Height 76.200
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Star G430 (2)
Tablet 0x28BD 0x0075 0x0000 0x0000 16 2 "TABLET G3 4x3"
Name "XP-Pen Star G430"
ReportLength 16
MaxX 45720
MaxY 29210
MaxPressure 2047
Width 101.600
Height 76.200
InitString 100
Type XPPen

# XP-Pen Star G430S V2
Tablet 0x28BD 0x0913 0x0000 0x0000 12
Name "XP-Pen Star G430S V2"
ReportLength 12
MaxX 10160
MaxY 7620
MaxPressure 8191
Width 101.600
Height 76.200
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star G430S (1)
Tablet 0x28BD 0x0075 0x0000 0x0000 8 2 "G430S"
Name "XP-Pen Star G430S"
ReportLength 8
MaxX 20320
MaxY 15240
MaxPressure 8191
Width 101.600
Height 76.200
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Star G430S (2)
Tablet 0x28BD 0x0913 0x0000 0x0000 14
Name "XP-Pen Star G430S"
ReportLength 14
MaxX 20320
MaxY 15240
MaxPressure 8191
Width 101.600
Height 76.200
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star G540 Pro
Tablet 0x28BD 0x0061 0x0000 0x0000 8
Name "XP-Pen Star G540 Pro"
ReportLength 8
MaxX 54720
MaxY 29210
MaxPressure 8191
Width 136.800
Height 73.025
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Star G540
Tablet 0x28BD 0x0075 0x0000 0x0000 8 2 "TABLET G3 5x4"
Name "XP-Pen Star G540"
ReportLength 8
MaxX 45720
MaxY 29210
MaxPressure 8191
Width 228.600
Height 146.050
InitReport 0x02 0xB0 0x02
Type XPPen

# XP-Pen Star G640 (Variant 2)
Tablet 0x28BD 0x0914 0x0000 0x0000 12
Name "XP-Pen Star G640 (Variant 2)"
ReportLength 12
MaxX 15999
MaxY 9999
MaxPressure 8191
Width 159.990
Height 99.990
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star G640 (1)
Tablet 0x28BD 0x0094 0x0000 0x0000 8
Name "XP-Pen Star G640"
ReportLength 8
MaxX 32000
MaxY 20000
MaxPressure 8191
Width 160.000
Height 100.000
InitReport 0x02 0xB0 0x02
InitString 100 110
Type XPPen

# XP-Pen Star G640 (2)
Tablet 0x28BD 0x0914 0x0000 0x0000 14
Name "XP-Pen Star G640"
ReportLength 14
MaxX 32000
MaxY 20000
MaxPressure 8191
Width 160.000
Height 100.000
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star G640S
Tablet 0x28BD 0x0906 0x0000 0x0000 10
Tablet 0x28BD 0x0906 0x0000 0x0000 12
Name "XP-Pen Star G640S"
ReportLength 10
MaxX 32999
MaxY 20599
MaxPressure 8191
Width 164.995
Height 102.995
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star G960
Tablet 0x28BD 0x0920 0x0000 0x0000 10
Name "XP-Pen Star G960"
ReportLength 10
MaxX 22352
MaxY 13970
MaxPressure 8191
Width 223.520
Height 139.700
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star G960S Plus
Tablet 0x28BD 0x0918 0x0000 0x0000 12
Name "XP-Pen Star G960S Plus"
ReportLength 12
MaxX 22860
MaxY 15240
MaxPressure 8191
Width 228.600
Height 152.400
InitReport 0x02 0xB0 0x04
Type XPPen

# XP-Pen Star G960S
Tablet 0x28BD 0x0917 0x0000 0x0000 12
Name "XP-Pen Star G960S"
ReportLength 12
MaxX 22860
MaxY 15240
MaxPressure 8191
Width 228.600
Height 152.400
InitReport 0x02 0xB0 0x04
Type XPPen

)CFG";

	const char* configDataHuionLegacy = R"CFG(
# Huion 420
Tablet "{62F12D4C-3431-4EFD-8DD7-8E9AAB18D30C}" 6 420
Name "Huion 420"
ReportId 0x07
ReportLength 8
DetectMask 0x80
MaxX 8340
MaxY 4680
MaxPressure 2047
Width 101.6
Height 56.6

# Huion H420
Tablet "{62F12D4C-3431-4EFD-8DD7-8E9AAB18D30C}" 6 H420
Name "Huion H420"
ReportLength 8
DetectMask 0x80
MaxX 8340
MaxY 4680
MaxPressure 2047
Width 101.6
Height 56.6

# Huion H430P
Tablet "{62F12D4C-3431-4EFD-8DD7-8E9AAB18D30C}" 201 HUION_T176
Name "Huion H430P"
ReportId 0x08
ReportLength 12
DetectMask 0x80
IgnoreMask 0x60
MaxX 24384
MaxY 15240
MaxPressure 4095
Width 121.92
Height 76.20

# Huion H640P
Tablet "{62F12D4C-3431-4EFD-8DD7-8E9AAB18D30C}" 201 HUION_T173
Name "Huion H640P"
ReportId 0x08
ReportLength 8
DetectMask 0x80
MaxX 31999
MaxY 20000
MaxPressure 8191
Width 160.0
Height 100.0

# Gaomon S56K
Tablet "{62F12D4C-3431-4EFD-8DD7-8E9AAB18D30C}" 201 HUION_T156
Name "Gaomon S56K"
ReportId 0x07
ReportLength 8
DetectMask 0x80
MaxX 25196
MaxY 18896
MaxPressure 2047
Width 160.0
Height 120.0

# Huion osu!tablet
Tablet "{62F12D4C-3431-4EFD-8DD7-8E9AAB18D30C}" 200 HVAN
Name "Huion osu!tablet (check the GitHub issue #99)"
ReportId 0x07
ReportLength 8
DetectMask 0x80
MaxX 8340
MaxY 4680
MaxPressure 2047
Width 101.6
Height 56.6

# === wacom.cfg ===

# Wacom PTK-450
Tablet 0x056A 0x0029 0xFF0D 0x0001
Tablet 0x056A 0x0029 0x000D 0x0001
Name "Wacom PTK-450"
ReportLength 10
DetectMask 0xE0
MaxX 31496
MaxY 19685
MaxPressure 2047
Width 157.480
Height 98.425
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom PTK-650
Tablet 0x056A 0x002A 0xFF0D 0x0001
Tablet 0x056A 0x002A 0x000D 0x0001
Name "Wacom PTK-650"
ReportLength 10
DetectMask 0xE0
MaxX 44704
MaxY 27940
MaxPressure 2047
Width 223.520
Height 139.700
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom PTH-450
Tablet 0x056A 0x0026 0xFF0D 0x0001
Tablet 0x056A 0x0026 0x000D 0x0001
Name "Wacom PTH-450"
ReportLength 10
DetectMask 0xE0
MaxX 31496
MaxY 19685
MaxPressure 2047
Width 157.480
Height 98.425
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom PTH-451
Tablet 0x056A 0x0314 0xFF0D 0x0001
Tablet 0x056A 0x0314 0x000D 0x0001
Name "Wacom PTH-451"
ReportLength 10
DetectMask 0x40
MaxX 31496
MaxY 19685
MaxPressure 2047
Width 157.480
Height 98.425
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom PTH-650
Tablet 0x056A 0x0027 0xFF0D 0x0001
Tablet 0x056A 0x0027 0x000D 0x0001
Name "Wacom PTH-650"
ReportLength 10
DetectMask 0xE0
MaxX 44704
MaxY 27940
MaxPressure 2047
Width 223.520
Height 139.700
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom PTH-651
Tablet 0x056A 0x0315 0xFF0D 0x0001
Tablet 0x056A 0x0315 0x000D 0x0001
Name "Wacom PTH-651"
ReportLength 10
DetectMask 0x40
MaxX 44704
MaxY 27940
MaxPressure 2047
Width 223.520
Height 139.700
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom PTH-850
Tablet 0x056A 0x0028 0xFF0D 0x0001
Tablet 0x056A 0x0028 0x000D 0x0001
Name "Wacom PTH-850"
ReportLength 10
DetectMask 0xE0
MaxX 65024
MaxY 40640
MaxPressure 2047
Width 325.120
Height 203.200
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom PTH-851
Tablet 0x056A 0x0317 0xFF0D 0x0001
Tablet 0x056A 0x0317 0x000D 0x0001
Name "Wacom PTH-851"
ReportLength 10
DetectMask 0x40
MaxX 65024
MaxY 40640
MaxPressure 2047
Width 325.120
Height 203.200
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom MTE-450
Tablet 0x056A 0x0065 0xFF0D 0x0001
Tablet 0x056A 0x0065 0x000D 0x0001
Name "Wacom MTE-450"
ReportLength 9
DetectMask 0x00
MaxX 14760
MaxY 9225
MaxPressure 511
Width 147.600
Height 92.250
InitFeature 0x02 0x02

# Wacom CTT-460
Tablet 0x056A 0x00D0 0xFF0D 0x0001
Tablet 0x056A 0x00D0 0x000D 0x0001
Name "Wacom CTT-460"
ReportLength 9
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.200
Height 92.000
InitFeature 0x02 0x02

# Wacom CTL-460
Tablet 0x056A 0x00D4 0xFF0D 0x0001
Tablet 0x056A 0x00D4 0x000D 0x0001
Name "Wacom CTL-460"
ReportLength 9
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.200
Height 92.000
InitFeature 0x02 0x02

# Wacom CTH-460(A)
Tablet 0x056A 0x00D6 0xFF0D 0x0001
Tablet 0x056A 0x00D6 0x000D 0x0001
Name "Wacom CTH-460(A)"
ReportLength 9
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.200
Height 92.000
InitFeature 0x02 0x02

# Wacom CTH-461(A)
Tablet 0x056A 0x00D7 0xFF0D 0x0001
Tablet 0x056A 0x00D7 0x000D 0x0001
Name "Wacom CTH-461(A)"
ReportLength 9
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.200
Height 92.000
InitFeature 0x02 0x02

# Wacom CTH-461SE
Tablet 0x056A 0x00DA 0xFF0D 0x0001
Tablet 0x056A 0x00DA 0x000D 0x0001
Name "Wacom CTH-461SE"
ReportLength 9
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.200
Height 92.000
InitFeature 0x02 0x02

# Wacom CTH-460
Tablet 0x056A 0x00D1 0xFF0D 0x0001
Tablet 0x056A 0x00D1 0x000D 0x0001
Name "Wacom CTH-460"
ReportLength 9
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.200
Height 92.000
InitFeature 0x02 0x02

# Wacom CTH-461
Tablet 0x056A 0x00D2 0xFF0D 0x0001
Tablet 0x056A 0x00D2 0x000D 0x0001
Name "Wacom CTH-461"
ReportLength 9
DetectMask 0x40
MaxX 14720
MaxY 9200
MaxPressure 1023
Width 147.200
Height 92.000
InitFeature 0x02 0x02

# Wacom CTL-660
Tablet 0x056A 0x00D5 0xFF0D 0x0001
Tablet 0x056A 0x00D5 0x000D 0x0001
Name "Wacom CTL-660"
ReportLength 9
DetectMask 0x40
MaxX 21648
MaxY 13700
MaxPressure 1023
Width 216.480
Height 137.000
InitFeature 0x02 0x02

# Wacom CTH-661
Tablet 0x056A 0x00D3 0xFF0D 0x0001
Tablet 0x056A 0x00D3 0x000D 0x0001
Name "Wacom CTH-661"
ReportLength 9
DetectMask 0x40
MaxX 21648
MaxY 13700
MaxPressure 1023
Width 216.480
Height 137.000
InitFeature 0x02 0x02

# Wacom CTH-661(A)
Tablet 0x056A 0x00D8 0xFF0D 0x0001
Tablet 0x056A 0x00D8 0x000D 0x0001
Name "Wacom CTH-661(A)"
ReportLength 9
DetectMask 0x40
MaxX 21648
MaxY 13700
MaxPressure 1023
Width 216.480
Height 137.000
InitFeature 0x02 0x02

# Wacom CTH-661SE
Tablet 0x056A 0x00DB 0xFF0D 0x0001
Tablet 0x056A 0x00DB 0x000D 0x0001
Name "Wacom CTH-661SE"
ReportLength 9
DetectMask 0x40
MaxX 21648
MaxY 13700
MaxPressure 1023
Width 216.480
Height 137.000
InitFeature 0x02 0x02

# Wacom CTL-690
Tablet 0x056A 0x033D 0xFF0D 0x0001
Tablet 0x056A 0x033D 0x000D 0x0001
Name "Wacom CTL-690"
ReportLength 10
DetectMask 0x40
MaxX 21600
MaxY 13500
MaxPressure 2047
Width 216.000
Height 135.000
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom PTK-440
Tablet 0x056A 0x00B8 0x0000 0x0000 10
Tablet 0x056A 0x00B8 0x000D 0x0001
Name "Wacom PTK-440"
ReportId 0x02
ReportLength 10
DetectMask 0x20
MaxX 31496
MaxY 19685
MaxPressure 2047
Width 157.480
Height 98.425
InitFeature 0x02 0x02
Type WacomIntuos4

# Wacom PTK-440 (Wacom drivers installed)
Tablet 0x056A 0x00B8 0x0000 0x0000 11
Name "Wacom PTK-440 (Wacom drivers installed)"
ReportId 0x02
ReportLength 11
DetectMask 0x20
MaxX 31496
MaxY 19685
MaxPressure 2047
Width 157.480
Height 98.425
InitFeature 0x02 0x02
Type WacomIntuos4

# Wacom CTH-690
Tablet 0x056A 0x033E 0xFF0D 0x0001
Tablet 0x056A 0x033E 0x000D 0x0001
Name "Wacom CTH-690"
ReportLength 10
DetectMask 0x40
MaxX 21600
MaxY 13500
MaxPressure 2047
Width 216.000
Height 135.000
InitFeature 0x02 0x02
Type WacomIntuos

# Wacom CTE-450
Tablet 0x056A 0x0017 0xFF0D 0x0001
Tablet 0x056A 0x0017 0x000D 0x0001
Name "Wacom CTE-450"
ReportId 0x02
ReportLength 9
DetectMask 0x00
MaxX 14760
MaxY 9225
MaxPressure 511
Width 147.600
Height 92.250
InitFeature 0x02 0x02
Type WacomBamboo

# Wacom CTE-650
Tablet 0x056A 0x0018 0x0000 0x0000 9
Tablet 0x056A 0x0018 0x0000 0x0000 10
Name "Wacom CTE-650 (native match)"
ReportId 0x02
ReportLength 9
DetectMask 0x00
MaxX 21648
MaxY 13530
MaxPressure 511
Width 216.480
Height 135.300
InitFeature 0x02 0x02
Type WacomBamboo

# Wacom CTE-650
Tablet 0x056A 0x0018 0x0000 0x0000 11
Tablet 0x056A 0x0018 0xFF00 0x000A
Tablet 0x056A 0x0018 0xFF00 0x0001
Tablet 0x056A 0x0018 0xFF00 0x0002
Name "Wacom CTE-650 (Wacom drivers installed)"
ReportId 0x02
ReportLength 11
DetectMask 0x40
MaxX 21648
MaxY 13530
MaxPressure 511
Width 216.480
Height 135.300
Type WacomDrivers

# Wacom CTE-650
Tablet 0x056A 0x0018 0xFF0D 0x0001
Tablet 0x056A 0x0018 0x000D 0x0001
Name "Wacom CTE-650"
ReportId 0x02
ReportLength 9
DetectMask 0x00
MaxX 21648
MaxY 13530
MaxPressure 511
Width 216.480
Height 135.300
InitFeature 0x02 0x02
Type WacomBamboo

)CFG";

	const char* configDataGaomonExtended = R"CFG(
# Gaomon extended configs
# Device string matching keeps shared VID/PID tablets from using the wrong size.

# Gaomon 1060 Pro
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM02_T174_\d{6}$"
Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "OEM02_T174_\d{6}$"
Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "OEM02_T174_\d{6}$"
Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "GM001_(T213|T216)_\d{6}$"
Tablet 0x256C 0x2004 0x0000 0x0000 12 201 "GM001_T240_\d{6}$"
Name "Gaomon 1060 Pro"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254.000
Height 158.750
InitString 200
Type UCLogic

# Gaomon GM116HD
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "(OEM02_M171|GM001_M21t)_\d{6}$"
Name "Gaomon GM116HD"
ReportLength 12
MaxX 51260
MaxY 28840
MaxPressure 8191
Width 256.300
Height 144.200
InitString 200
Type Giano

# Gaomon GM156HD
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM02_M166_\d{6}$"
Name "Gaomon GM156HD"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344.200
Height 193.600
InitString 200
Type Giano

# Gaomon M106K Pro
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM02_T19m_\d{6}$"
Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "OEM02_T19m_\d{6}$"
Name "Gaomon M106K Pro"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254.000
Height 158.750
InitString 200
Type UCLogic

# Gaomon M106K
Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "OEM02_T151_\d{6}$"
Name "Gaomon M106K"
ReportLength 12
MaxX 40000
MaxY 25000
MaxPressure 2047
Width 200.000
Height 125.000
InitString 200
Type UCLogic

# Gaomon M10K Pro
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM02_T19n_\d{6}$"
Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "OEM02_T19n_\d{6}$"
Name "Gaomon M10K Pro"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254.000
Height 158.750
InitString 200
Type UCLogic

# Gaomon M10K
Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "OEM02_T17b_\d{6}$"
Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "OEM02_T17b_\d{6}$"
Name "Gaomon M10K"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254.000
Height 158.750
InitString 200
Type UCLogic

# Gaomon M1220
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "GM001_T201_\d{6}$"
Name "Gaomon M1220"
ReportLength 12
MaxX 51700
MaxY 32310
MaxPressure 8191
Width 258.500
Height 161.550
InitString 200
Type UCLogic

# Gaomon M1230
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "GM001_T202_\d{6}$"
Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "GM001_T202_\d{6}$"
Name "Gaomon M1230"
ReportLength 12
MaxX 51700
MaxY 34305
MaxPressure 8191
Width 258.500
Height 171.525
InitString 200
Type UCLogic

# Gaomon M5
Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "OEM02_T195_\d{6}$"
Name "Gaomon M5"
ReportLength 12
MaxX 35830
MaxY 23700
MaxPressure 8191
Width 179.150
Height 118.500
InitString 200
Type UCLogic

# Gaomon M6
Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "(OEM02_T183|GM001_T223)_\d{6}$"
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM02_T183_\d{6}$"
Name "Gaomon M6"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254.000
Height 158.750
InitString 200
Type UCLogicV2

# Gaomon M7
Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "GM001_(T220|T207)_\d{6}$"
Name "Gaomon M7"
ReportLength 12
MaxX 51689
MaxY 34308
MaxPressure 16383
Width 258.445
Height 171.540
InitString 200
Type UCLogicV2

# Gaomon M8 (Variant 2)
Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "GM001_T221_\d{6}$"
Name "Gaomon M8 (Variant 2)"
ReportLength 12
MaxX 51693
MaxY 32307
MaxPressure 16383
Width 258.465
Height 161.535
InitString 200
Type UCLogicV2

# Gaomon M8
Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "GM001_T208_\d{6}$"
Name "Gaomon M8"
ReportLength 12
MaxX 51693
MaxY 32307
MaxPressure 8191
Width 258.465
Height 161.535
InitString 200
Type UCLogicV2

# Gaomon PD1161
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM02_M191_\d{6}$"
Name "Gaomon PD1161"
ReportLength 12
MaxX 51264
MaxY 28836
MaxPressure 8191
Width 256.320
Height 144.180
InitString 200
Type UCLogic

# Gaomon PD1320
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "GM001_M201_\d{6}$"
Name "Gaomon PD1320"
ReportLength 12
MaxX 58752
MaxY 33048
MaxPressure 8191
Width 293.760
Height 165.240
InitString 200
Type Giano

# Gaomon PD156 Pro
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM02_M19h_\d{6}$"
Name "Gaomon PD156 Pro"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344.200
Height 193.600
InitString 200
Type Giano

# Gaomon PD1560
Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "OEM02_M177_\d{6}$"
Name "Gaomon PD1560"
ReportLength 12
MaxX 68832
MaxY 38718
MaxPressure 8191
Width 344.160
Height 193.590
InitString 200
Type Giano

# Gaomon PD1561
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM02_M196_\d{6}$"
Name "Gaomon PD1561"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344.200
Height 193.600
InitString 200
Type Giano

# Gaomon PD2200
Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "OEM02_M198_\d{6}$"
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "GM001_M226_\d{6}$"
Name "Gaomon PD2200"
ReportLength 12
MaxX 95345
MaxY 53645
MaxPressure 8191
Width 476.725
Height 268.225
InitString 200
Type Giano

# Gaomon S56K
Tablet 0x256C 0x006D 0x0000 0x0000 8 201 "HUION_T156_\d{6}$"
Tablet 0x256C 0x006E 0x0000 0x0000 8 201 "HUION_T156_\d{6}$"
Tablet 0x256C 0x006E 0x0000 0x0000 16 201 "HUION_T156_\d{6}$"
Name "Gaomon S56K"
ReportLength 8
MaxX 32000
MaxY 24000
MaxPressure 2047
Width 160.000
Height 120.000
InitString 200

# Gaomon S620
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM02_T18e_\d{6}$"
Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "OEM02_T18e_\d{6}$"
Name "Gaomon S620"
ReportLength 12
MaxX 33020
MaxY 20320
MaxPressure 8191
Width 165.100
Height 101.600
InitString 200
Type UCLogic

# Gaomon S630
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "GM001_T203_\d{6}$"
Name "Gaomon S630"
ReportLength 12
MaxX 25860
MaxY 16160
MaxPressure 8191
Width 129.300
Height 80.800
InitString 200
Type UCLogic

# Gaomon S830
Tablet 0x256C 0x006D 0x0000 0x0000 12 2 "Gaomon Tablet_ S830"
Name "Gaomon S830"
ReportLength 12
MaxX 34460
MaxY 21540
MaxPressure 8191
Width 172.300
Height 107.700
InitString 200
Type UCLogic

)CFG";

	std::vector<std::string> commands;
	auto appendCommands = [&commands](const char* data) {
		std::istringstream stream(data);
		std::string line;
		while (std::getline(stream, line)) {
			
			if (line.empty() || line[0] == '#' || line[0] == '\r' || line[0] == '\n')
				continue;
			
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();
			if (!line.empty())
				commands.push_back(line);
		}
	};
	appendCommands(configData);
	appendCommands(configDataWacomExtended);
	appendCommands(configDataXpPenExtended1);
	appendCommands(configDataXpPenExtended2);
	appendCommands(configDataHuionLegacy);
	appendCommands(configDataGaomonExtended);
	return commands;
}

} 
