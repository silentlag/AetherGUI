#include "stdafx.h"
#include "StatusSharedWriter.h"
#include <atomic>

using namespace AetherShared;

StatusSharedWriter::StatusSharedWriter() {}
StatusSharedWriter::~StatusSharedWriter() { Shutdown(); }

bool StatusSharedWriter::Initialize() {
	if (hMapping) return true;

	size_t total = TotalSize();
	hMapping = CreateFileMappingW(
		INVALID_HANDLE_VALUE,
		nullptr,
		PAGE_READWRITE,
		0, (DWORD)total,
		kMappingName);
	if (!hMapping) return false;

	void* view = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, total);
	if (!view) {
		CloseHandle(hMapping);
		hMapping = nullptr;
		return false;
	}

	header = (Header*)view;
	header->magic       = kMagic;
	header->version     = kVersion;
	header->capacity    = kPosRingCapacity;
	header->reserved0   = 0;
	header->writeIndex  = 0;
	header->latency     = {};
	for (auto& r : header->reserved1) r = 0;

	ring = (PosSlot*)((char*)view + sizeof(Header));
	for (uint32_t i = 0; i < kPosRingCapacity; ++i) ring[i] = {};

	return true;
}

void StatusSharedWriter::Shutdown() {
	if (header) { UnmapViewOfFile(header); header = nullptr; ring = nullptr; }
	if (hMapping) { CloseHandle(hMapping); hMapping = nullptr; }
}

void StatusSharedWriter::PushPos(uint64_t tsNs, float x, float y, float pressure, float hz, bool tipDown) {
	if (!header) return;

	auto* wp = reinterpret_cast<std::atomic<uint64_t>*>(&header->writeIndex);
	uint64_t w = wp->load(std::memory_order_relaxed);

	PosSlot& slot = ring[w % kPosRingCapacity];
	slot.timestampNs = tsNs;
	slot.x           = x;
	slot.y           = y;
	slot.pressure    = pressure;
	slot.hz          = hz;
	slot.flags       = tipDown ? 1u : 0u;
	slot.reserved    = 0;

	wp->store(w + 1, std::memory_order_release);
}

void StatusSharedWriter::PushLatency(float avgMs, float p99Ms, float maxMs, int samples) {
	if (!header) return;

	header->latency.avgMs   = avgMs;
	header->latency.p99Ms   = p99Ms;
	header->latency.maxMs   = maxMs;
	header->latency.samples = samples;
}
