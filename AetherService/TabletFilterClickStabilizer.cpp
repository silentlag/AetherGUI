#include "stdafx.h"
#include "TabletFilterClickStabilizer.h"

#define LOG_MODULE "ClickStabilizer"
#include "Logger.h"

TabletFilterClickStabilizer::TabletFilterClickStabilizer() {
	enabled = false;
	clickStabilizeMs = 8.0;
	wasTipDown = false;
}

TabletFilterClickStabilizer::~TabletFilterClickStabilizer() {}

void TabletFilterClickStabilizer::Reset(Vector2D pos) {
	position.Set(pos);
	target.Set(pos);
	latchedPos.Set(pos);
	wasTipDown = false;
}

void TabletFilterClickStabilizer::SetTarget(Vector2D vector, double h) {
	target.Set(vector);
	z = h;
}

void TabletFilterClickStabilizer::SetPosition(Vector2D vector, double h) {
	position.Set(vector);
	z = h;
}

bool TabletFilterClickStabilizer::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

void TabletFilterClickStabilizer::SetReportState(BYTE buttons, double pressure, double hoverDistance) {

	bool tipDown = (buttons & 0x01) != 0;

	if (tipDown && !wasTipDown) {

		latchedPos.Set(target);
		latchStart = std::chrono::high_resolution_clock::now();
	}
	wasTipDown = tipDown;
}

void TabletFilterClickStabilizer::Update() {
	if (!enabled || !wasTipDown || clickStabilizeMs <= 0.0) {
		position.Set(target);
		return;
	}

	auto now = std::chrono::high_resolution_clock::now();
	double elapsedMs = std::chrono::duration<double, std::milli>(now - latchStart).count();
	if (elapsedMs < clickStabilizeMs) {

		position.Set(latchedPos);
	}
	else {

		position.Set(target);
	}
}
