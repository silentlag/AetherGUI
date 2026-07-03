#pragma once

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterLazyMouse : public TabletFilter {
public:

	double radius;

	double smooth;

	TabletFilterLazyMouse();
	~TabletFilterLazyMouse();

	void SetTarget(Vector2D vector, double h) override;
	void SetPosition(Vector2D vector, double h) override;
	void SetReportState(BYTE buttons, double pressure, double hoverDistance) override;
	bool GetPosition(Vector2D *outputVector) override;
	void Update() override;
	void Reset(Vector2D position) override;

private:
	Vector2D position;
	Vector2D target;
	Vector2D cursor;
	bool hasLatch;
	BYTE prevButtons;
	bool hasReport;
	BYTE lastButtons;
};
