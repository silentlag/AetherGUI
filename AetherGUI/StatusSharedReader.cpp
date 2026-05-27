#include "StatusSharedReader.h"
#include <atomic>

using namespace AetherShared;

StatusSharedReader::StatusSharedReader() {}
StatusSharedReader::~StatusSharedReader() { Close(); }

bool StatusSharedReader::TryOpen() {
	if (header) return true;

	hMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, kMappingName);
	if (!hMapping) return false;

	void* view = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, TotalSize());
	if (!view) {
		CloseHandle(hMapping);
		hMapping = nullptr;
		return false;
	}

	Header* h = (Header*)view;

	
	if (h->magic != kMagic || h->version != kVersion || h->capacity != kPosRingCapacity) {
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		hMapping = nullptr;
		return false;
	}

	header = h;
	ring   = (const PosSlot*)((char*)view + sizeof(Header));

	
	auto* wp = reinterpret_cast<volatile uint64_t*>(&header->writeIndex);
	readIndex = *wp;
	return true;
}

void StatusSharedReader::Close() {
	if (header) { UnmapViewOfFile((LPCVOID)header); header = nullptr; ring = nullptr; }
	if (hMapping) { CloseHandle(hMapping); hMapping = nullptr; }
	readIndex = 0;
}

bool StatusSharedReader::ReadLatency(LatencyBlock& out) const {
	if (!header) return false;
	out = header->latency;
	return true;
}
