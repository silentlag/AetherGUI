#include "stdafx.h"
#include "TabletFilterPeak.h"

#define LOG_MODULE "Peak"
#include "Logger.h"




TabletFilterPeak::TabletFilterPeak() {

	
	buffer.SetLength(3);

	
	distanceThreshold = 10;
}





TabletFilterPeak::~TabletFilterPeak() {
}






void TabletFilterPeak::Reset(Vector2D targetVector) {
	buffer.Reset();
}

void TabletFilterPeak::SetTarget(Vector2D targetVector, double h) {
	buffer.Add(targetVector);
}

void TabletFilterPeak::SetPosition(Vector2D vector, double h) {
	position.x = vector.x;
	position.y = vector.y;
}

bool TabletFilterPeak::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

void TabletFilterPeak::Update() {
	Vector2D oldPosition;
	double distance;

	
	if (
		buffer.GetLatest(&oldPosition, -1)
		&&
		buffer.GetLatest(&position, 0)
		) {

		
		distance = oldPosition.Distance(position);
		if (distance > distanceThreshold) {

			







			position.x = oldPosition.x;
			position.y = oldPosition.y;

			
			buffer.Reset();

		}
	}
}