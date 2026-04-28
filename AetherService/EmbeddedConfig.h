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
ReportLength 12
DetectMask 0x80
MaxX 31999
MaxY 20000
MaxPressure 8191
Width 160.0
Height 100.0
Type UCLogic

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

	const char* configDataHuionHid = R"CFG(
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T173_\d{6}$"
Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T173_\d{6}$"
Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T203_\d{6}$"
Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T203_\d{6}$"
Name "Huion H640P"
ReportId 0x08
ReportLength 12
DetectMask 0x80
MaxX 32000
MaxY 20000
MaxPressure 8191
Width 160.0
Height 100.0
InitString 200
Type UCLogic

)CFG";

	const char* configDataParblo = R"CFG(
Tablet 0x0B57 0x9091 0x0000 0x0000 10
Name "Parblo A609"
ReportLength 10
DetectMask 0x20
MaxX 44704
MaxY 27940
MaxPressure 2047
Width 223.520
Height 139.700
InitFeature 0x02 0x02
Type WacomIntuos

Tablet 0x0483 0xA022 0x0000 0x0000 10
Name "Parblo A610 Pro (Variant 2)"
ReportLength 10
MaxX 25200
MaxY 15750
MaxPressure 8191
Width 252.000
Height 157.500
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x28BD 0x1903 0x0000 0x0000 10
Name "Parblo A610 Pro"
ReportLength 10
MaxX 51100
MaxY 32000
MaxPressure 8191
Width 255.500
Height 160.000
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x5543 0x0081 0x0000 0x0000 8 201 "F401-HK708-STD"
Name "Parblo A610"
ReportLength 8
MaxX 40000
MaxY 24000
MaxPressure 2047
Width 254.000
Height 152.400
InitString 100 123
Type XPPenOffsetAux

Tablet 0x0483 0xA640 0x0000 0x0000 10
Name "Parblo A640 V2"
ReportLength 10
MaxX 31203
MaxY 18730
MaxPressure 8191
Width 156.015
Height 93.650
InitReport 0x02 0xB0 0x04 0x00 0x00 0x00 0x00 0x00
Type XPPen

Tablet 0x5543 0x0061 0x0000 0x0000 10
Name "Parblo A640"
ReportLength 10
MaxX 29616
MaxY 17170
MaxPressure 8191
Width 148.080
Height 85.850
InitString 100

Tablet 0x0483 0xA013 0x0000 0x0000 10
Name "Parblo Intangbo M"
ReportLength 10
MaxX 26050
MaxY 16000
MaxPressure 8191
Width 260.500
Height 160.000
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x0483 0xA014 0x0000 0x0000 10
Name "Parblo Intangbo S"
ReportLength 10
MaxX 17306
MaxY 10045
MaxPressure 8191
Width 173.060
Height 100.450
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x0483 0xA016 0x0000 0x0000 10
Name "Parblo Intangbo SW"
ReportLength 10
MaxX 28800
MaxY 16200
MaxPressure 16383
Width 182.880
Height 102.869
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x0483 0xA005 0x0000 0x0000 10
Name "Parblo Ninos M"
ReportLength 10
MaxX 20320
MaxY 10160
MaxPressure 8191
Width 217.195
Height 125.220
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x0483 0xA019 0x0000 0x0000 10
Name "Parblo Ninos N4"
ReportLength 10
MaxX 21600
MaxY 16200
MaxPressure 8191
Width 108.000
Height 81.000
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x0483 0xA020 0x0000 0x0000 10
Name "Parblo Ninos N7"
ReportLength 10
MaxX 25200
MaxY 15745
MaxPressure 8191
Width 177.800
Height 111.090
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x0483 0xA20B 0x0000 0x0000 10
Name "Parblo Ninos N7B"
ReportLength 10
MaxX 28799
MaxY 16199
MaxPressure 8191
Width 177.800
Height 114.000
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x0483 0xA006 0x0000 0x0000 10
Name "Parblo Ninos S"
ReportLength 10
MaxX 24743
MaxY 16380
MaxPressure 8191
Width 157.118
Height 104.013
InitReport 0x02 0xB0 0x04
Type XPPen

)CFG";
	


	const char* configDataCompatibleExtended1 = R"CFG(
Tablet 0x28BD 0x0910 0x0000 0x0000 10
Name "Adesso Cybertablet K8"
ReportLength 10
MaxX 20320
MaxY 11430
MaxPressure 8191
Width 203,2
Height 114,3
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM16_T206_\d{6}$"
Name "Artisul A1201"
ReportLength 12
MaxX 51690
MaxY 34310
MaxPressure 8191
Width 258,45
Height 171,55
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "OEM16_T206_\d{6}$"
Name "Artisul A1201"
ReportLength 12
MaxX 51690
MaxY 34310
MaxPressure 8191
Width 258,45
Height 171,55
InitString 200
Type UCLogic

Tablet 0x5543 0x0054 0x0000 0x0000 64
Name "Artisul AP604"
ReportLength 64
MaxX 15900
MaxY 9930
MaxPressure 2047
Width 159
Height 99,3

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "OEM16_M193_\d{6}$"
Name "Artisul D16 Pro"
ReportLength 12
MaxX 68834
MaxY 38714
MaxPressure 8191
Width 344,17
Height 193,57
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "OEM16_M165_\d{6}$"
Name "Artisul D22S"
ReportLength 12
MaxX 95352
MaxY 53645
MaxPressure 8191
Width 476,76
Height 268,225
InitString 200
Type Giano

Tablet 0x5543 0x0081 0x0000 0x0000 10 201 "F610_M61P-T\d{4}$"
Name "Artisul M0610 Pro"
ReportLength 10
MaxX 51475
MaxY 32298
MaxPressure 8191
Width 257,375
Height 161,49
InitString 100

Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "OEM16_T205_\d{6}$"
Name "Artisul M0610 Pro"
ReportLength 12
MaxX 51475
MaxY 32298
MaxPressure 8191
Width 257,375
Height 161,49
InitString 100

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "OEM16_T205_\d{6}$"
Name "Artisul M0610 Pro"
ReportLength 12
MaxX 51475
MaxY 32298
MaxPressure 8191
Width 257,375
Height 161,49
InitString 100

Tablet 0x256C 0x006E 0x0000 0x0000 16 6 "1060PRO" 121 "HA60-F400"
Name "Huion 1060 Plus"
ReportLength 16
MaxX 40000
MaxY 25000
MaxPressure 2047
Width 254
Height 158,75
InitString 100 123
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T161_\d{6}$"
Name "Huion G10T"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x0061 0x0000 0x0000 12 201 "HUION_T209_\d{6}$"
Name "Huion G930L"
ReportLength 12
MaxX 69088
MaxY 43180
MaxPressure 8191
Width 345,44
Height 215,9
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T166_\d{6}$"
Name "Huion GC610"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T166_\d{6}$"
Name "Huion GC610"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M174_\d{6}$"
Name "Huion GT-156HD V2"
ReportLength 12
MaxX 68780
MaxY 38710
MaxPressure 8191
Width 343,9
Height 193,55
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M(168|18a)_\d{6}$"
Name "Huion GT-191 V2"
ReportLength 12
MaxX 86976
MaxY 47736
MaxPressure 8191
Width 434,88
Height 238,68
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 8 201 "HUION_M165_\d{6}$"
Name "Huion GT-220 V2 (2048)"
ReportLength 8
MaxX 37540
MaxY 21120
MaxPressure 2047
Width 476,758
Height 268,224
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M165_\d{6}$"
Name "Huion GT-220 V2"
ReportLength 12
MaxX 95336
MaxY 53629
MaxPressure 8191
Width 476,68
Height 268,145
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M167_\d{6}$"
Name "Huion GT-221 Pro"
ReportLength 12
MaxX 95346
MaxY 53641
MaxPressure 8191
Width 476,73
Height 268,205
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M175_\d{6}$"
Name "Huion GT-221"
ReportLength 12
MaxX 95346
MaxY 53641
MaxPressure 8191
Width 476,73
Height 268,205
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T(167|205)_\d{6}$"
Name "Huion H1060P"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T167_\d{6}$"
Name "Huion H1060P"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T(205|219)_\d{6}$"
Name "Huion H1060P"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x0068 0x0000 0x0000 12 201 "HUION_T(21m|255)_\d{6}$"
Name "Huion H1061P"
ReportLength 12
MaxX 53340
MaxY 33340
MaxPressure 8191
Width 266,7
Height 166,7
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T191_\d{6}$"
Name "Huion H1161"
ReportLength 12
MaxX 55880
MaxY 34925
MaxPressure 8191
Width 279,4
Height 174,625
InitString 200
Type UCLogicV2

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T191_\d{6}$"
Name "Huion H1161"
ReportLength 12
MaxX 55880
MaxY 34925
MaxPressure 8191
Width 279,4
Height 174,625
InitString 200
Type UCLogicV2

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T198_\d{6}$"
Name "Huion H320M"
ReportLength 12
MaxX 45700
MaxY 28580
MaxPressure 8191
Width 228,5
Height 142,9
InitString 200
Type UCLogic

Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "HUION_T198_\d{6}$"
Name "Huion H320M"
ReportLength 12
MaxX 45700
MaxY 28580
MaxPressure 8191
Width 228,5
Height 142,9
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T210_\d{6}$"
Name "Huion H420X"
ReportLength 12
MaxX 21200
MaxY 13200
MaxPressure 8191
Width 106
Height 66
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_(T210|T223)_\d{6}$"
Name "Huion H420X"
ReportLength 12
MaxX 21200
MaxY 13200
MaxPressure 8191
Width 106
Height 66
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T(211|224)_\d{6}$"
Name "Huion H580X"
ReportLength 12
MaxX 40640
MaxY 25400
MaxPressure 8191
Width 203,2
Height 127
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T(211|224)_\d{6}$"
Name "Huion H580X"
ReportLength 12
MaxX 40640
MaxY 25400
MaxPressure 8191
Width 203,2
Height 127
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T184_\d{6}$"
Name "Huion H610 Pro V2"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T184_\d{6}$"
Name "Huion H610 Pro V2"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T184_\d{6}$"
Name "Huion H610 Pro V2"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 16 121 "^HA60\x00*$"
Name "Huion H610 Pro V3"
ReportLength 16
MaxX 2048
MaxY 2048
MaxPressure 2047
Width 254
Height 158,75

Tablet 0x256C 0x006E 0x0000 0x0000 8 121 "^HA60\x00*$"
Name "Huion H610 Pro V3"
ReportLength 8
MaxX 2048
MaxY 2048
MaxPressure 2047
Width 254
Height 158,75

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T175_\d{6}$"
Name "Huion H610 Pro"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T175_\d{6}$"
Name "Huion H610 Pro"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_(T212|T229)_\d{6}$"
Name "Huion H610X"
ReportLength 12
MaxX 50800
MaxY 31760
MaxPressure 8191
Width 254
Height 158,8
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T212_\d{6}$"
Name "Huion H610X"
ReportLength 12
MaxX 50800
MaxY 31760
MaxPressure 8191
Width 254
Height 158,8
InitString 200
Type UCLogic

Tablet 0x256C 0x0066 0x0000 0x0000 12 201 "HUION_T(21j|253)_\d{6}$"
Name "Huion H641P"
ReportLength 12
MaxX 32000
MaxY 20000
MaxPressure 8191
Width 160
Height 100
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T19g_\d{6}$"
Name "Huion H642"
ReportLength 12
MaxX 32004
MaxY 19812
MaxPressure 8191
Width 160,02
Height 99,06
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T19g_\d{6}$"
Name "Huion H642"
ReportLength 12
MaxX 32004
MaxY 19812
MaxPressure 8191
Width 160,02
Height 99,06
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 8 6 "HuionH690"
Name "Huion H690"
ReportLength 8
MaxX 36000
MaxY 22500
MaxPressure 2047
Width 228,6
Height 142,875
InitString 100 123
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 16 6 "HuionH690"
Name "Huion H690"
ReportLength 16
MaxX 36000
MaxY 22500
MaxPressure 2047
Width 228,6
Height 142,875
InitString 100 123
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T22d_\d{6}$"
Name "Huion H950P"
ReportLength 12
MaxX 44200
MaxY 27600
MaxPressure 8191
Width 221
Height 138
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T(172|204)_\d{6}$"
Name "Huion H950P"
ReportLength 12
MaxX 44200
MaxY 27600
MaxPressure 8191
Width 221
Height 138
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T(172|204)_\d{6}$"
Name "Huion H950P"
ReportLength 12
MaxX 44200
MaxY 27600
MaxPressure 8191
Width 221
Height 138
InitString 200
Type UCLogic

Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "HUION_T204_\d{6}$"
Name "Huion H950P"
ReportLength 12
MaxX 44200
MaxY 27600
MaxPressure 8191
Width 221
Height 138
InitString 200
Type UCLogic

Tablet 0x256C 0x0067 0x0000 0x0000 12 201 "HUION_(T21k|T254)_\d{6}$"
Name "Huion H951P"
ReportLength 12
MaxX 44200
MaxY 27600
MaxPressure 8191
Width 221
Height 138
InitString 200
Type Inspiroy

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T226_\d{6}$"
Name "Huion HC16 (Variant 2)"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 16383
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T18C_\d{6}$"
Name "Huion HC16"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T18C_\d{6}$"
Name "Huion HC16"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T194_\d{6}$"
Name "Huion HS610"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogicV2

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T(194|227)_\d{6}$"
Name "Huion HS610"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogicV2

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T19c_\d{6}$"
Name "Huion HS611"
ReportLength 12
MaxX 51680
MaxY 32300
MaxPressure 8191
Width 258,4
Height 161,5
InitString 200
Type UCLogic

Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "HUION_T19c_\d{6}$"
Name "Huion HS611"
ReportLength 12
MaxX 51680
MaxY 32300
MaxPressure 8191
Width 258,4
Height 161,5
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T(181|193)_\d{6}$"
Name "Huion HS64"
ReportLength 12
MaxX 32000
MaxY 20400
MaxPressure 8191
Width 160
Height 102
InitString 200
Type UCLogic
)CFG";

	const char* configDataCompatibleExtended2 = R"CFG(
Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T181_\d{6}$"
Name "Huion HS64"
ReportLength 12
MaxX 32000
MaxY 20400
MaxPressure 8191
Width 160
Height 102
InitString 200
Type UCLogic

Tablet 0x256C 0x006F 0x0000 0x0000 12 201 "HUION_T225_\d{6}$"
Name "Huion HS64"
ReportLength 12
MaxX 32000
MaxY 20400
MaxPressure 8191
Width 160
Height 102
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T206_\d{6}$"
Name "Huion HS95"
ReportLength 12
MaxX 40638
MaxY 25398
MaxPressure 8191
Width 203,19
Height 126,99
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M19p_\d{6}$"
Name "Huion Kamvas 12"
ReportLength 12
MaxX 53580
MaxY 33640
MaxPressure 8191
Width 267,9
Height 168,2
InitString 200
Type Giano

Tablet 0x256C 0x2008 0x0000 0x0000 14 201 "HUION_M22c_\d{6}$"
Name "Huion Kamvas 13 (Gen 3)"
ReportLength 14
MaxX 58760
MaxY 33040
MaxPressure 16383
Width 293,8
Height 165,2
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_(?:M20h|M19f|M215)_\d{6}$"
Name "Huion Kamvas 13"
ReportLength 12
MaxX 58752
MaxY 33048
MaxPressure 8191
Width 293,76
Height 165,24
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M19s_\d{6}$"
Name "Huion Kamvas 16 (2021)"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344,2
Height 193,6
InitString 200
Type Giano

Tablet 0x256C 0x2009 0x0000 0x0000 14 201 "HUION_M22d_\d{6}$"
Name "Huion Kamvas 16 (Gen 3)"
ReportLength 14
MaxX 69920
MaxY 39340
MaxPressure 16383
Width 349,6
Height 196,7
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M18e_\d{6}$"
Name "Huion Kamvas 16"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344,2
Height 193,6
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M192_\d{6}$"
Name "Huion Kamvas 20"
ReportLength 12
MaxX 86950
MaxY 47750
MaxPressure 8191
Width 434,75
Height 238,75
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M19t_\d{6}$"
Name "Huion Kamvas 22 Plus"
ReportLength 12
MaxX 95328
MaxY 53622
MaxPressure 8191
Width 476,64
Height 268,11
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M19g_\d{6}$"
Name "Huion Kamvas 22"
ReportLength 12
MaxX 95352
MaxY 53645
MaxPressure 8191
Width 476,76
Height 268,225
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M205_\d{6}$"
Name "Huion Kamvas 24 Plus"
ReportLength 12
MaxX 105370
MaxY 59270
MaxPressure 8191
Width 526,85
Height 296,35
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M206_\d{6}$"
Name "Huion Kamvas 24"
ReportLength 12
MaxX 105370
MaxY 59270
MaxPressure 8191
Width 526,85
Height 296,35
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M171_\d{6}$"
Name "Huion Kamvas Pro 12"
ReportLength 12
MaxX 53580
MaxY 33640
MaxPressure 8191
Width 267,9
Height 168,2
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M(20j|171)_\d{6}$"
Name "Huion Kamvas Pro 12"
ReportLength 12
MaxX 53580
MaxY 33640
MaxPressure 8191
Width 267,9
Height 168,2
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M(210|213)_\d{6}$"
Name "Huion Kamvas Pro 13 (2.5k)"
ReportLength 12
MaxX 57293
MaxY 35808
MaxPressure 8191
Width 286,465
Height 179,04
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M(182|20k)_\d{6}$"
Name "Huion Kamvas Pro 13"
ReportLength 12
MaxX 58752
MaxY 33048
MaxPressure 8191
Width 293,76
Height 165,24
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M(182|20k)_\d{6}$"
Name "Huion Kamvas Pro 13"
ReportLength 12
MaxX 58752
MaxY 33048
MaxPressure 8191
Width 293,76
Height 165,24
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M214_\d{6}$"
Name "Huion Kamvas Pro 16 (2.5k)"
ReportLength 12
MaxX 69926
MaxY 39333
MaxPressure 8191
Width 349,63
Height 196,665
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M20q_\d{6}$"
Name "Huion Kamvas Pro 16 (2.5k)"
ReportLength 12
MaxX 69926
MaxY 39333
MaxPressure 8191
Width 349,63
Height 196,665
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M202_\d{6}$"
Name "Huion Kamvas Pro 16 (4k)"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344,2
Height 193,6
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M20a_\d{6}$"
Name "Huion Kamvas Pro 16 Plus (4k)"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344,2
Height 193,6
InitString 200
Type Giano

Tablet 0x256C 0x2000 0x0000 0x0000 14 201 "HUION_M246_\d{6}$"
Name "Huion Kamvas Pro 16 V2"
ReportLength 14
MaxX 68840
MaxY 38720
MaxPressure 16383
Width 344,2
Height 193,6
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M183_\d{6}$"
Name "Huion Kamvas Pro 16"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344,2
Height 193,6
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M20m_\d{6}$"
Name "Huion Kamvas Pro 16"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344,2
Height 193,6
InitString 200
Type Giano

Tablet 0x256C 0x006B 0x0000 0x0000 14 201 "HUION_M220_\d{6}$"
Name "Huion Kamvas Pro 19 (4K)"
ReportLength 14
MaxX 81792
MaxY 46006
MaxPressure 16383
Width 408,96
Height 230,03
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M(189|193)_\d{6}$"
Name "Huion Kamvas Pro 20"
ReportLength 12
MaxX 87075
MaxY 47750
MaxPressure 8191
Width 435,375
Height 238,75
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M194_\d{6}$"
Name "Huion Kamvas Pro 22 (2019)"
ReportLength 12
MaxX 95350
MaxY 53644
MaxPressure 8191
Width 476,75
Height 268,22
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M207_\d{6}$"
Name "Huion Kamvas Pro 24 (4K)"
ReportLength 12
MaxX 105370
MaxY 59270
MaxPressure 8191
Width 526,85
Height 296,35
InitString 200
Type Giano

Tablet 0x256C 0x006C 0x0000 0x0000 14 201 "HUION_M23b_\d{6}$"
Name "Huion Kamvas Pro 24 (Gen 3)"
ReportLength 14
MaxX 105408
MaxY 59290
MaxPressure 16383
Width 527,04
Height 296,45
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_M184_\d{6}$"
Name "Huion Kamvas Pro 24"
ReportLength 12
MaxX 105379
MaxY 59236
MaxPressure 8191
Width 526,895
Height 296,18
InitString 200
Type Giano

Tablet 0x256C 0x006C 0x0000 0x0000 14 201 "HUION_M221_\d{6}$"
Name "Huion Kamvas Pro 27"
ReportLength 14
MaxX 119347
MaxY 67132
MaxPressure 16383
Width 596,735
Height 335,66
InitString 200
Type Giano

Tablet 0x256C 0x2011 0x0000 0x0000 12 2 "Huion Tablet_L310"
Name "Huion L310"
ReportLength 12
MaxX 32000
MaxY 20000
MaxPressure 16383
Width 160
Height 100
InitString 200
Type Inspiroy

Tablet 0x256C 0x2012 0x0000 0x0000 12 201 "HUION_T23b_\d{6}$"
Name "Huion L610"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 16383
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 16 201 "HUION_T151_\d{6}$"
Name "Huion New 1060 Plus (2048)"
ReportLength 16
MaxX 40000
MaxY 25000
MaxPressure 2047
Width 254
Height 158,75
InitString 100
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 8 201 "HUION_T151_\d{6}$"
Name "Huion New 1060 Plus (2048)"
ReportLength 8
MaxX 40000
MaxY 25000
MaxPressure 2047
Width 254
Height 158,75
InitString 100
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T174_\d{6}$"
Name "Huion New 1060 Plus"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T174_\d{6}$"
Name "Huion New 1060 Plus"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T218_\d{6}$"
Name "Huion Note X10"
ReportLength 12
MaxX 37400
MaxY 28200
MaxPressure 8191
Width 187
Height 141
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T185_\d{6}$"
Name "Huion Q11K V2"
ReportLength 12
MaxX 55880
MaxY 34925
MaxPressure 8191
Width 279,4
Height 174,625
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T164_\d{6}$"
Name "Huion Q11K"
ReportLength 12
MaxX 55880
MaxY 34925
MaxPressure 8191
Width 279,4
Height 174,625
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T164_\d{6}$"
Name "Huion Q11K"
ReportLength 12
MaxX 55880
MaxY 34925
MaxPressure 8191
Width 279,4
Height 174,625
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T18d_\d{6}$"
Name "Huion Q620M"
ReportLength 12
MaxX 53340
MaxY 33020
MaxPressure 8191
Width 266,7
Height 165,1
InitString 200
Type Giano

Tablet 0x256C 0x0060 0x0000 0x0000 12 201 "HUION_T(216|22b)_\d{6}$"
Name "Huion Q630M"
ReportLength 12
MaxX 53340
MaxY 33340
MaxPressure 8191
Width 266,7
Height 166,7
InitString 200
Type Giano

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_M211_\d{6}$"
Name "Huion RDS-160"
ReportLength 12
MaxX 68840
MaxY 38720
MaxPressure 8191
Width 344,2
Height 193,6
InitString 200
Type Giano

Tablet 0x256C 0x2001 0x0000 0x0000 12 201 "HUION_M225_\d{6}$"
Name "Huion RDS-220"
ReportLength 12
MaxX 95352
MaxY 53645
MaxPressure 8191
Width 476,76
Height 268,225
InitString 200
Type Giano

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T217_\d{6}$"
Name "Huion RTE-100"
ReportLength 12
MaxX 24384
MaxY 15238
MaxPressure 8191
Width 121,92
Height 76,19
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T19h_\d{6}$"
Name "Huion RTM-500"
ReportLength 12
MaxX 44199
MaxY 27599
MaxPressure 8191
Width 220,995
Height 137,995
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T19h_\d{6}$"
Name "Huion RTM-500"
ReportLength 12
MaxX 44199
MaxY 27599
MaxPressure 8191
Width 220,995
Height 137,995
InitString 200
Type UCLogic

Tablet 0x256C 0x006D 0x0000 0x0000 12 201 "HUION_T19k_\d{6}$"
Name "Huion RTP-700"
ReportLength 12
MaxX 55880
MaxY 34920
MaxPressure 8191
Width 279,4
Height 174,6
InitString 200
Type UCLogic

Tablet 0x256C 0x0064 0x0000 0x0000 12 201 "HUION_T19k_\d{6}$"
Name "Huion RTP-700"
ReportLength 12
MaxX 55880
MaxY 34920
MaxPressure 8191
Width 279,4
Height 174,6
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 64 201 "HUION_T153_\d{6}$"
Name "Huion WH1409 V2 (Variant 2)"
ReportLength 64
MaxX 55200
MaxY 34498
MaxPressure 2047
Width 350,52
Height 219,062
InitString 200
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T188_\d{6}$"
Name "Huion WH1409 V2"
ReportLength 12
MaxX 70000
MaxY 43600
MaxPressure 8191
Width 350
Height 218
InitString 200
Type Giano

Tablet 0x256C 0x006E 0x0000 0x0000 12 201 "HUION_T153_\d{6}$"
Name "Huion WH1409"
ReportLength 12
MaxX 70000
MaxY 43600
MaxPressure 8191
Width 350
Height 218
InitString 200
Type Giano

Tablet 0x5543 0x0004 0x0000 0x0000 8
Name "KENTING K5540"
ReportLength 8
MaxX 10999
MaxY 7999
MaxPressure 1023
Width 139,687
Height 101,587
InitString 100
Type UCLogic

Tablet 0x6161 0x4D15 0x0000 0x0000 12 201 "Vision_V201_\d{6}"
Name "LetSketch WP9620C"
ReportLength 12
MaxX 53889
MaxY 33591
MaxPressure 8191
Width 269,445
Height 167,955
InitString 200
Type UCLogic
)CFG";

	const char* configDataCompatibleExtended3 = R"CFG(
Tablet 0x256C 0x006E 0x0000 0x0000 16 6 "10594" 121 "HA60-F400"
Name "Monoprice 10594"
ReportLength 16
MaxX 40000
MaxY 25000
MaxPressure 2047
Width 254
Height 158,75
InitString 100 123
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 16 6 "^10594\x00\x00et\x00$"
Name "Monoprice 10594"
ReportLength 16
MaxX 40000
MaxY 25000
MaxPressure 2047
Width 254
Height 158,75
InitString 100 123
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 8 6 "10594" 121 "HA60-F400"
Name "Monoprice 10594"
ReportLength 8
MaxX 40000
MaxY 25000
MaxPressure 2047
Width 254
Height 158,75
InitString 100 123
Type UCLogic

Tablet 0x5543 0x0781 0x0000 0x0000 8
Name "Monoprice MP1060-HA60"
ReportLength 8
MaxX 39999
MaxY 24999
MaxPressure 1023
Width 253,994
Height 158,744
InitString 100
Type UCLogic

Tablet 0x256C 0x006E 0x0000 0x0000 8 6 "Huion" 121 "M508"
Name "Turcom TS-6580"
ReportLength 8
MaxX 32000
MaxY 20000
MaxPressure 2047
Width 203,2
Height 127
InitString 100

Tablet 0x256C 0x006E 0x0000 0x0000 16 6 "Huion" 121 "M508"
Name "Turcom TS-6580"
ReportLength 16
MaxX 32000
MaxY 20000
MaxPressure 2047
Width 203,2
Height 127
InitString 100

Tablet 0x5543 0x0081 0x0000 0x0000 8 201 "F600_A60n_E\d{4}$"
Name "UC-Logic 1060N"
ReportLength 8
MaxX 50800
MaxY 30480
MaxPressure 2047
Width 254
Height 152,4
InitReport 0x02 0xB0 0x02
InitString 100
Type XPPen

Tablet 0x5543 0x0081 0x0000 0x0000 8 201 "F600_A60n_E\d{4}$"
Name "UC-Logic 1060N"
ReportLength 8
MaxX 50800
MaxY 30480
MaxPressure 2047
Width 254
Height 152,4
InitReport 0x02 0xB0 0x02
InitString 100
Type XPPenOffsetAux

Tablet 0x5543 0x0042 0x0000 0x0000 8 2 "TabletPF1209"
Name "UC-Logic PF1209"
ReportLength 8
MaxX 47999
MaxY 35999
MaxPressure 1023
Width 304,794
Height 228,594
InitString 109
Type UCLogic

Tablet 0x5543 0x0071 0x0000 0x0000 8 3 "F600_A62_140327"
Name "UC-Logic TWMNA62"
ReportLength 8
MaxX 40000
MaxY 25000
MaxPressure 2047
Width 254
Height 158,75
InitString 100
Type XPPen

Tablet 0x28BD 0x0924 0x0000 0x0000 10 4 "UG901_BPU1006"
Name "UGEE M708 V2"
ReportLength 10
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254
Height 152,4
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x28BD 0x0924 0x0000 0x0000 12
Name "UGEE M708 V2"
ReportLength 12
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254
Height 152,4
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x28BD 0x0924 0x0000 0x0000 12 4 "UG902_BPG1006"
Name "UGEE M708 V3"
ReportLength 12
MaxX 50800
MaxY 30480
MaxPressure 16383
Width 254
Height 152,4
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x5543 0x0081 0x0000 0x0000 8 4 "F401-20111028" 5 "UC-LOIC"
Name "UGEE M708"
ReportLength 8
MaxX 40000
MaxY 24000
MaxPressure 2047
Width 200
Height 120
InitString 100 123
Type XPPenOffsetAux

Tablet 0x28BD 0x2903 0x0000 0x0000 12
Name "UGEE M808"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 8191
Width 254
Height 158,75
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x28BD 0x2902 0x0000 0x0000 12
Name "UGEE M908"
ReportLength 12
MaxX 50800
MaxY 31750
MaxPressure 16383
Width 254
Height 158,75
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x28BD 0x0938 0x0000 0x0000 12
Name "UGEE S1060"
ReportLength 12
MaxX 50800
MaxY 32000
MaxPressure 8191
Width 254
Height 160
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x28BD 0x0937 0x0000 0x0000 12
Name "UGEE S640"
ReportLength 12
MaxX 31998
MaxY 19994
MaxPressure 8191
Width 159,99
Height 99,97
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x28BD 0x093B 0x0000 0x0000 12
Name "UGEE U1200"
ReportLength 12
MaxX 52644
MaxY 29611
MaxPressure 8191
Width 263,22
Height 148,055
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x28BD 0x093C 0x0000 0x0000 12
Name "UGEE U1600"
ReportLength 12
MaxX 68828
MaxY 38714
MaxPressure 8191
Width 344,14
Height 193,57
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x28BD 0x290C 0x0000 0x0000 12
Name "UGEE UE16"
ReportLength 12
MaxX 68199
MaxY 38399
MaxPressure 16383
Width 340,995
Height 191,995
InitReport 0x02 0xB0 0x04
Type XPPen

Tablet 0x2FEB 0x0006 0x0000 0x0000 9
Name "VEIKK A15 Pro"
ReportOffset 1
ReportLength 9
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254
Height 152,4
InitReport 0x09 0x01 0x04

Tablet 0x2FEB 0x0002 0x0000 0x0000 9
Name "VEIKK A30"
ReportOffset 1
ReportLength 9
MaxX 50800
MaxY 30480
MaxPressure 8191
Width 254
Height 152,4
InitReport 0x09 0x01 0x04

Tablet 0x0531 0x0100 0x0000 0x0000 192
Name "Wacom CTC-4110WL"
ReportLength 192
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152
Height 95
InitFeature 0x02 0x02
Type WacomIntuosV3

Tablet 0x0531 0x0102 0x0000 0x0000 192
Name "Wacom CTC-6110WL"
ReportLength 192
MaxX 21600
MaxY 13500
MaxPressure 4095
Width 216
Height 135
InitFeature 0x02 0x02
Type WacomIntuosV3

Tablet 0x0531 0x0103 0x0000 0x0000 19
Name "Wacom CTC-6110WL"
ReportLength 19
MaxX 21600
MaxY 13500
MaxPressure 4095
Width 216
Height 135
InitFeature 0x02 0x02
Type WacomIntuosV3

Tablet 0x056A 0x006A 0x0000 0x0000 8
Name "Wacom CTE-460"
ReportLength 8
MaxX 15200
MaxY 9500
MaxPressure 511
Width 152
Height 95
InitFeature 0x02 0x02
Type WacomBamboo

Tablet 0x056A 0x0016 0x0000 0x0000 8
Name "Wacom CTE-640"
ReportLength 8
MaxX 16704
MaxY 12064
MaxPressure 511
Width 208,8
Height 150,8
InitFeature 0x02 0x02
Type WacomBamboo

Tablet 0x056A 0x006B 0x0000 0x0000 8
Name "Wacom CTE-660"
ReportLength 8
MaxX 21648
MaxY 13500
MaxPressure 511
Width 216,48
Height 135
InitFeature 0x02 0x02
Type WacomBamboo

Tablet 0x056A 0x0069 0x0000 0x0000 8
Name "Wacom CTF-430"
ReportLength 8
MaxX 5104
MaxY 3712
MaxPressure 511
Width 127,6
Height 92,8
InitFeature 0x02 0x02
Type WacomBamboo

Tablet 0x056A 0x0376 0x0000 0x0000 64
Name "Wacom CTL-4100WL"
ReportLength 64
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152
Height 95
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x0376 0x0000 0x0000 192
Name "Wacom CTL-4100WL"
ReportLength 192
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152
Height 95
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x0376 0x0000 0x0000 193
Name "Wacom CTL-4100WL"
ReportOffset 1
ReportLength 193
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152
Height 95
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x0377 0x0000 0x0000 361
Name "Wacom CTL-4100WL"
ReportLength 361
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152
Height 95
Type WacomIntuosV2

Tablet 0x056A 0x0377 0x0000 0x0000 193
Name "Wacom CTL-4100WL"
ReportOffset 1
ReportLength 193
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152
Height 95
Type WacomIntuosV2

Tablet 0x056A 0x03C5 0x0000 0x0000 192
Name "Wacom CTL-4100WL"
ReportLength 192
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152
Height 95
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x03C5 0x0000 0x0000 193
Name "Wacom CTL-4100WL"
ReportOffset 1
ReportLength 193
MaxX 15200
MaxY 9500
MaxPressure 4095
Width 152
Height 95
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x0375 0x0000 0x0000 192
Name "Wacom CTL-6100"
ReportLength 192
MaxX 21600
MaxY 13500
MaxPressure 4095
Width 216
Height 135
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x0378 0x0000 0x0000 192
Name "Wacom CTL-6100WL"
ReportLength 192
MaxX 21600
MaxY 13500
MaxPressure 4095
Width 216
Height 135
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x03C7 0x0000 0x0000 192
Name "Wacom CTL-6100WL"
ReportLength 192
MaxX 21600
MaxY 13500
MaxPressure 4095
Width 216
Height 135
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x03CE 0x0000 0x0000 192
Name "Wacom DTC-121"
ReportLength 192
MaxX 25632
MaxY 14418
MaxPressure 4095
Width 256,32
Height 144,18
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x03A6 0x0000 0x0000 192
Name "Wacom DTC-133"
ReportLength 192
MaxX 29434
MaxY 16556
MaxPressure 4095
Width 294,34
Height 165,56
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x03A6 0x0000 0x0000 193
Name "Wacom DTC-133"
ReportOffset 1
ReportLength 193
MaxX 29434
MaxY 16556
MaxPressure 4095
Width 294,34
Height 165,56
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x034F 0x0000 0x0000 192
Name "Wacom DTH-1320"
ReportLength 192
MaxX 59552
MaxY 33848
MaxPressure 8191
Width 297,76
Height 169,24
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x03F0 0x0000 0x0000 192
Name "Wacom Movink 13 (DTH-135)"
ReportLength 192
MaxX 59552
MaxY 33848
MaxPressure 8191
Width 297,76
Height 169,24
InitFeature 0x02 0x02
Type WacomIntuosV3

Tablet 0x056A 0x03D0 0x0000 0x0000 192
Name "Wacom Cintiq Pro 22 (DTH-227)"
ReportLength 192
MaxX 96012
MaxY 54356
MaxPressure 8191
Width 480,06
Height 271,78
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x03C0 0x0000 0x0000 192
Name "Wacom Cintiq Pro 27 (DTH-271)"
ReportLength 192
MaxX 120032
MaxY 67868
MaxPressure 8191
Width 600,16
Height 339,34
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x0304 0x0000 0x0000 10
Name "Wacom Cintiq 13HD (DTK-1300)"
ReportLength 10
MaxX 59800
MaxY 34200
MaxPressure 2047
Width 299
Height 171
InitFeature 0x02 0x02
Type WacomIntuos

Tablet 0x056A 0x0390 0x0000 0x0000 192
Name "Wacom Cintiq 16 (DTK-1660)"
ReportLength 192
MaxX 69632
MaxY 39518
MaxPressure 8191
Width 348,16
Height 197,59
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x03AE 0x0000 0x0000 192
Name "Wacom Cintiq 16 (DTK-1660)"
ReportLength 192
MaxX 69632
MaxY 39518
MaxPressure 8191
Width 348,16
Height 197,59
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x0010 0x0000 0x0000 9
Name "Wacom ET-0405-U"
ReportOffset 1
ReportLength 9
MaxX 10208
MaxY 7424
MaxPressure 511
Width 127,6
Height 92,8
InitFeature 0x02 0x02

Tablet 0x056A 0x0060 0x0000 0x0000 8
Name "Wacom FT-0405-U"
ReportLength 8
MaxX 5104
MaxY 3712
MaxPressure 511
Width 127,6
Height 92,8
InitFeature 0x02 0x02
Type WacomBamboo

Tablet 0x056A 0x0020 0x0000 0x0000 10
Name "Wacom GD-0405-U"
ReportLength 10
MaxX 25400
MaxY 21200
MaxPressure 2046
Width 127
Height 106
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0020 0x0000 0x0000 11
Name "Wacom GD-0405-U"
ReportOffset 1
ReportLength 11
MaxX 25400
MaxY 21200
MaxPressure 2046
Width 127
Height 106
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0021 0x0000 0x0000 10
Name "Wacom GD-0608-U"
ReportLength 10
MaxX 40640
MaxY 32480
MaxPressure 2046
Width 203,2
Height 162,4
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0021 0x0000 0x0000 11
Name "Wacom GD-0608-U"
ReportOffset 1
ReportLength 11
MaxX 40640
MaxY 32480
MaxPressure 2046
Width 203,2
Height 162,4
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0022 0x0000 0x0000 10
Name "Wacom GD-0912-U"
ReportLength 10
MaxX 60960
MaxY 48120
MaxPressure 2046
Width 304,8
Height 240,6
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0022 0x0000 0x0000 11
Name "Wacom GD-0912-U"
ReportOffset 1
ReportLength 11
MaxX 60960
MaxY 48120
MaxPressure 2046
Width 304,8
Height 240,6
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0023 0x0000 0x0000 10
Name "Wacom GD-1212-U"
ReportLength 10
MaxX 60960
MaxY 63360
MaxPressure 2046
Width 304,8
Height 316,8
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0023 0x0000 0x0000 11
Name "Wacom GD-1212-U"
ReportOffset 1
ReportLength 11
MaxX 60960
MaxY 63360
MaxPressure 2046
Width 304,8
Height 316,8
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0024 0x0000 0x0000 10
Name "Wacom GD-1218-U"
ReportLength 10
MaxX 91440
MaxY 63360
MaxPressure 2046
Width 457,2
Height 316,8
InitFeature 0x04 0x00
Type WacomIntuos
)CFG";

	const char* configDataCompatibleExtended4 = R"CFG(
Tablet 0x056A 0x0024 0x0000 0x0000 11
Name "Wacom GD-1218-U"
ReportOffset 1
ReportLength 11
MaxX 91440
MaxY 63360
MaxPressure 2046
Width 457,2
Height 316,8
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0392 0x0000 0x0000 192
Name "Wacom PTH-460"
ReportLength 192
MaxX 31920
MaxY 19950
MaxPressure 8191
Width 159,6
Height 99,75
InitFeature 0x02 0x02
Type WacomIntuosV2

Tablet 0x056A 0x0392 0x0000 0x0000 193
Name "Wacom PTH-460"
ReportOffset 1
ReportLength 193
MaxX 31920
MaxY 19950
MaxPressure 8191
Width 159,6
Height 99,75
Type WacomIntuosV2

Tablet 0x056A 0x03DC 0x0000 0x0000 192
Name "Wacom PTH-460"
ReportLength 192
MaxX 31920
MaxY 19950
MaxPressure 8191
Width 159,6
Height 99,75
Type WacomIntuosV2

Tablet 0x056A 0x0357 0x0000 0x0000 192
Name "Wacom PTH-660"
ReportLength 192
MaxX 44800
MaxY 29600
MaxPressure 8191
Width 224
Height 148
Type WacomIntuosV2

Tablet 0x056A 0x0360 0x0000 0x0000 361
Name "Wacom PTH-660"
ReportLength 361
MaxX 44800
MaxY 29600
MaxPressure 8191
Width 224
Height 148

Tablet 0x056A 0x0358 0x0000 0x0000 192
Name "Wacom PTH-860"
ReportLength 192
MaxX 62200
MaxY 43200
MaxPressure 8191
Width 311
Height 216
Type WacomIntuosV2

Tablet 0x056A 0x0358 0x0000 0x0000 193
Name "Wacom PTH-860"
ReportOffset 1
ReportLength 193
MaxX 62200
MaxY 43200
MaxPressure 8191
Width 311
Height 216
Type WacomIntuosV2

Tablet 0x056A 0x00BB 0x0000 0x0000 10
Name "Wacom PTK-1240"
ReportLength 10
MaxX 97536
MaxY 60960
MaxPressure 2047
Width 487,68
Height 304,8
InitFeature 0x02 0x02
Type WacomIntuos4

Tablet 0x056A 0x00BB 0x0000 0x0000 11
Name "Wacom PTK-1240"
ReportOffset 1
ReportLength 11
MaxX 97536
MaxY 60960
MaxPressure 2047
Width 487,68
Height 304,8
InitFeature 0x02 0x02
Type WacomIntuos4

Tablet 0x056A 0x03F5 0x0000 0x0000 192
Name "Wacom PTK-470"
ReportLength 192
MaxX 37400
MaxY 21000
MaxPressure 8191
Width 187
Height 105
InitFeature 0x02 0x02
Type WacomIntuosV3

Tablet 0x056A 0x00BC 0x0000 0x0000 10
Name "Wacom PTK-540WL"
ReportLength 10
MaxX 40640
MaxY 25400
MaxPressure 2047
Width 203,2
Height 127
InitFeature 0x02 0x02
Type WacomIntuos4

Tablet 0x056A 0x00BC 0x0000 0x0000 11
Name "Wacom PTK-540WL"
ReportOffset 1
ReportLength 11
MaxX 40640
MaxY 25400
MaxPressure 2047
Width 203,2
Height 127
InitFeature 0x02 0x02
Type WacomIntuos4

Tablet 0x056A 0x00B9 0x0000 0x0000 10
Name "Wacom PTK-640"
ReportLength 10
MaxX 44704
MaxY 27940
MaxPressure 2047
Width 223,52
Height 139,7
InitFeature 0x02 0x02
Type WacomIntuos4

Tablet 0x056A 0x00B9 0x0000 0x0000 11
Name "Wacom PTK-640"
ReportOffset 1
ReportLength 11
MaxX 44704
MaxY 27940
MaxPressure 2047
Width 223,52
Height 139,7
InitFeature 0x02 0x02
Type WacomIntuos4

Tablet 0x056A 0x03F7 0x0000 0x0000 192
Name "Wacom PTK-670"
ReportLength 192
MaxX 52600
MaxY 29600
MaxPressure 8191
Width 263
Height 148
InitFeature 0x02 0x02
Type WacomIntuosV3

Tablet 0x056A 0x00BA 0x0000 0x0000 10
Name "Wacom PTK-840"
ReportLength 10
MaxX 65024
MaxY 40640
MaxPressure 2047
Width 325,12
Height 203,2
InitFeature 0x02 0x02
Type WacomIntuos4

Tablet 0x056A 0x00BA 0x0000 0x0000 11
Name "Wacom PTK-840"
ReportOffset 1
ReportLength 11
MaxX 65024
MaxY 40640
MaxPressure 2047
Width 325,12
Height 203,2
InitFeature 0x02 0x02
Type WacomIntuos4

Tablet 0x056A 0x03F9 0x0000 0x0000 192
Name "Wacom PTK-870"
ReportLength 192
MaxX 69800
MaxY 39000
MaxPressure 8191
Width 349
Height 195
InitFeature 0x02 0x02
Type WacomIntuosV3

Tablet 0x056A 0x0041 0x0000 0x0000 10
Name "Wacom XD-0405-U"
ReportLength 10
MaxX 25400
MaxY 21200
MaxPressure 2046
Width 127
Height 106
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0041 0x0000 0x0000 11
Name "Wacom XD-0405-U"
ReportOffset 1
ReportLength 11
MaxX 25400
MaxY 21200
MaxPressure 2046
Width 127
Height 106
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0042 0x0000 0x0000 10
Name "Wacom XD-0608-U"
ReportLength 10
MaxX 40640
MaxY 32480
MaxPressure 2046
Width 203,2
Height 162,4
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0042 0x0000 0x0000 11
Name "Wacom XD-0608-U"
ReportOffset 1
ReportLength 11
MaxX 40640
MaxY 32480
MaxPressure 2046
Width 203,2
Height 162,4
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0043 0x0000 0x0000 10
Name "Wacom XD-0912-U"
ReportLength 10
MaxX 60960
MaxY 48120
MaxPressure 2046
Width 304,8
Height 240,6
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0043 0x0000 0x0000 11
Name "Wacom XD-0912-U"
ReportOffset 1
ReportLength 11
MaxX 60960
MaxY 48120
MaxPressure 2046
Width 304,8
Height 240,6
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0044 0x0000 0x0000 10
Name "Wacom XD-1212-U"
ReportLength 10
MaxX 60960
MaxY 63360
MaxPressure 2046
Width 304,8
Height 316,8
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0044 0x0000 0x0000 11
Name "Wacom XD-1212-U"
ReportOffset 1
ReportLength 11
MaxX 60960
MaxY 63360
MaxPressure 2046
Width 304,8
Height 316,8
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0045 0x0000 0x0000 10
Name "Wacom XD-1218-U"
ReportLength 10
MaxX 91440
MaxY 63360
MaxPressure 2046
Width 457,2
Height 316,8
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos

Tablet 0x056A 0x0045 0x0000 0x0000 11
Name "Wacom XD-1218-U"
ReportOffset 1
ReportLength 11
MaxX 91440
MaxY 63360
MaxPressure 2046
Width 457,2
Height 316,8
InitFeature 0x02 0x02
InitFeature 0x04 0x00
Type WacomIntuos
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
	appendCommands(configDataHuionHid);
	appendCommands(configDataParblo);
	appendCommands(configDataCompatibleExtended1);
	appendCommands(configDataCompatibleExtended2);
	appendCommands(configDataCompatibleExtended3);
	appendCommands(configDataCompatibleExtended4);
appendCommands(configDataGaomonExtended);
	return commands;
}

} 





