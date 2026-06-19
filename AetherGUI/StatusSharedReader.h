#pragma once

#include <windows.h>
#include <atomic>
#include "AetherShared.h"

class StatusSharedReader {
public:
	StatusSharedReader();
	~StatusSharedReader();

	bool TryOpen();
	void Close();

	bool IsActive() const { return header != nullptr; }

	template<typename Fn>
	int Drain(Fn cb);

	bool ReadLatency(AetherShared::LatencyBlock& out) const;

private:
	HANDLE                       hMapping = nullptr;
	AetherShared::Header*        header   = nullptr;
	const AetherShared::PosSlot* ring     = nullptr;
	uint64_t                     readIndex = 0;
};

template<typename Fn>
int StatusSharedReader::Drain(Fn cb) {
	if (!header) return 0;

	auto* wp = reinterpret_cast<volatile uint64_t*>(&header->writeIndex);
	uint64_t w = *wp;
	std::atomic_thread_fence(std::memory_order_acquire);

	if (w == readIndex) return 0;

	uint64_t lagging = w - readIndex;
	if (lagging > AetherShared::kPosRingCapacity) {
		readIndex = w - AetherShared::kPosRingCapacity;
		lagging   = AetherShared::kPosRingCapacity;
	}

	int delivered = 0;
	while (readIndex < w) {
		const AetherShared::PosSlot& s = ring[readIndex % AetherShared::kPosRingCapacity];
		cb(s);
		++readIndex;
		++delivered;
	}
	return delivered;
}
