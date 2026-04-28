#pragma once
class TabletSettings {
public:

	enum TabletType {
		TabletNormal,
		TypeWacomIntuos,
		TypeWacom4100,
		TypeWacomBamboo,
		TypeWacomIntuos4,
		TypeWacomIntuosV2,
		TypeWacomIntuosV3,
		TypeUCLogic,
		TypeUCLogicV1,
		TypeUCLogicV2,
		TypeInspiroy,
		TypeGiano,
		TypeXPPen,
		TypeXPPenOffsetPressure,
		TypeXPPenGen2,
		TypeXPPenOffsetAux,
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
	int reportOffset;
	double skew;
	TabletType type;
	int mouseWheelSpeed;

	
	double invMaxX;
	double invMaxY;

	TabletSettings();
	~TabletSettings();
};
