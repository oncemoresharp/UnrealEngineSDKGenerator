#include <windows.h>

#include "ObjectsStore.hpp"

#include "EngineClasses.hpp"

// Lineage 2's Core.dll exports the global object array directly, so it can be
// resolved by symbol instead of a fragile byte-pattern scan:
//   ?GObjObjects@UObject@@1V?$TArray@PAVUObject@@@@A
//     -> private: static class TArray<class UObject*> UObject::GObjObjects
TArray<UObject*>* GlobalObjects = nullptr;

bool ObjectsStore::Initialize()
{
	auto core = GetModuleHandleW(L"Core.dll");
	if (core == nullptr)
	{
		return false;
	}

	GlobalObjects = reinterpret_cast<TArray<UObject*>*>(
		GetProcAddress(core, "?GObjObjects@UObject@@1V?$TArray@PAVUObject@@@@A"));

	if (GlobalObjects == nullptr)
	{
		// Ordinal fallback (ordinal 1377 in the analysed Core.dll export table).
		GlobalObjects = reinterpret_cast<TArray<UObject*>*>(
			GetProcAddress(core, MAKEINTRESOURCEA(1377)));
	}

	return GlobalObjects != nullptr;
}

void* ObjectsStore::GetAddress()
{
	return GlobalObjects;
}

size_t ObjectsStore::GetObjectsNum() const
{
	return GlobalObjects->Num();
}

UEObject ObjectsStore::GetById(size_t id) const
{
	return (*GlobalObjects)[id];
}
