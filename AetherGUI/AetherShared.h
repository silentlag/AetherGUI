#pragma once

#include <cstdint>

namespace AetherShared {

constexpr wchar_t kMappingName[]   = L"Local\\AetherStatusV1";
constexpr uint32_t kMagic          = 0x53746541;
constexpr uint32_t kVersion        = 1;
constexpr uint32_t kPosRingCapacity = 4096;

#pragma pack(push, 4)
struct PosSlot {
	uint64_t timestampNs;
	float    x;
	float    y;
	float    pressure;
	float    hz;
	uint32_t flags;
	uint32_t reserved;
};
#pragma pack(pop)
static_assert(sizeof(PosSlot) == 32, "PosSlot size must be 32 bytes");

#pragma pack(push, 4)
struct LatencyBlock {
	float avgMs;
	float p99Ms;
	float maxMs;
	int32_t samples;
};
#pragma pack(pop)

#pragma pack(push, 4)
struct Header {
	uint32_t magic;
	uint32_t version;
	uint32_t capacity;
	uint32_t reserved0;

	uint64_t writeIndex;

	LatencyBlock latency;

	uint64_t reserved1[4];
};
#pragma pack(pop)

inline size_t TotalSize() {
	return sizeof(Header) + sizeof(PosSlot) * kPosRingCapacity;
}

}
