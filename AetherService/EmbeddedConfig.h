#pragma once
#include <string>
#include <vector>
#include <sstream>

// Embedded tablet configurations — used as fallback when .cfg files are missing
namespace EmbeddedConfig {

// Returns all commands that would normally come from init.cfg + tablet.cfg + wacom.cfg
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

# Wacom CTL-480 (Wacom drivers installed)
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

# Wacom CTH-480 (Wacom drivers installed)
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
ReportLength 9
DetectMask 0x00
MaxX 14760
MaxY 9225
MaxPressure 511
Width 147.600
Height 92.250
InitFeature 0x02 0x02

# Wacom CTE-650
Tablet 0x056A 0x0018 0x0000 0x0000 9
Name "Wacom CTE-650 (OpenTabletDriver match)"
ReportLength 9
DetectMask 0x00
MaxX 21648
MaxY 13530
MaxPressure 511
Width 216.480
Height 135.300
InitFeature 0x02 0x02

# Wacom CTE-650
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
ReportLength 9
DetectMask 0x00
MaxX 21648
MaxY 13530
MaxPressure 511
Width 216.480
Height 135.300
InitFeature 0x02 0x02

)CFG";

	std::vector<std::string> commands;
	std::istringstream stream(configData);
	std::string line;
	while (std::getline(stream, line)) {
		// Skip empty lines and comments
		if (line.empty() || line[0] == '#' || line[0] == '\r' || line[0] == '\n')
			continue;
		// Trim trailing \r
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
			line.pop_back();
		if (!line.empty())
			commands.push_back(line);
	}
	return commands;
}

} // namespace EmbeddedConfig
