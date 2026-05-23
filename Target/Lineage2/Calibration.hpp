#pragma once

#include <cstddef>
#include <cstdint>

// Runtime-resolved field offsets for the Lineage 2 reflection meta classes.
//
// UObject is fully pinned by the snapshot, but the exact byte offsets of the
// fields the generator walks on UStruct/UField/UProperty vary between Lineage 2
// builds. CalibrateOffsets() discovers them from the live "Class Core.Object"
// data; the values below are the canonical UE2 defaults used as a fallback.
namespace L2
{
	extern size_t Off_UField_SuperField;	// default 0x34
	extern size_t Off_UField_Next;			// default 0x38
	extern size_t Off_UStruct_Children;		// default 0x48
	extern size_t Off_UStruct_PropertySize;	// default 0x4C
	extern size_t Off_UProperty_Offset;		// default 0x54

	// Probes the live game for the offsets above. Returns true when every
	// offset was confirmed; on a partial failure the unverified offsets keep
	// their defaults and the caller can still attempt a dump.
	bool CalibrateOffsets();

	template<typename T>
	inline T ReadAt(const void* base, size_t offset)
	{
		return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + offset);
	}
}
