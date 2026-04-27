#pragma once
class TabletSettings {
public:

	enum TabletType {
		TabletNormal,
		TypeWacomIntuos,
		TypeWacom4100,
		TypeWacomBamboo,
		TypeWacomIntuos4,
		TypeWacomDrivers
	};

	BYTE detectMask;
	BYTE ignoreMask;
	int maxX;
	int maxY;
	int maxPressure;
	int clickPressure;
	int keepTipDown;
	double width;
	double height;
	BYTE reportId;
	int reportLength;
	double skew;
	TabletType type;
	int mouseWheelSpeed;

	// Pre-computed reciprocals for fast coordinate conversion
	double invMaxX;
	double invMaxY;

	TabletSettings();
	~TabletSettings();
};
