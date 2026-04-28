#include "stdafx.h"
#include "PositionRingBuffer.h"





PositionRingBuffer::PositionRingBuffer() {
	maxLength = sizeof(buffer) / sizeof(Vector2D);
	length = 0;
	count = 0;
	index = 0;
	isValid = false;
}





PositionRingBuffer::~PositionRingBuffer() {
}





void PositionRingBuffer::SetLength(int len) {
	if(len > maxLength) {
		length = maxLength;
	} else {
		length = len;
	}
}





void PositionRingBuffer::Add(Vector2D vector) {
	buffer[index].x = vector.x;
	buffer[index].y = vector.y;
	index++;
	count++;
	if(count > length) {
		count = length;
	}
	if(index >= length) {
		index = 0;
	}
	isValid = true;
}





bool PositionRingBuffer::GetLatest(Vector2D *output, int delta) {
	int newIndex;

	
	if(count == 0) return false;

	
	if(delta > 0 || delta <= -count) return false;

	newIndex = index - 1 + delta;

	
	if(newIndex < 0) newIndex = count + newIndex;

	if(newIndex < 0 || newIndex >= count) {
		return false;
	}

	output->x = buffer[newIndex].x;
	output->y = buffer[newIndex].y;
	return true;
}





void PositionRingBuffer::Reset() {
	count = 0;
	index = 0;
	isValid = false;
}






Vector2D *PositionRingBuffer::operator[](std::size_t index) {
	return &(buffer[index]);
}