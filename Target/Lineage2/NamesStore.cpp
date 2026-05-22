#include <windows.h>

#include "NamesStore.hpp"

#include "EngineClasses.hpp"

// Lineage 2's Core.dll exports the global name array directly:
//   ?Names@FName@@0V?$TArray@PAUFNameEntry@@@@A
//     -> private: static class TArray<struct FNameEntry*> FName::Names
//
// Only the wide name string of an FNameEntry is needed. Its byte offset inside
// the entry differs between Lineage 2 builds, so it is probed at runtime.
struct FNameEntry
{
	unsigned char Raw[1];
};

TArray<FNameEntry*>* GlobalNames = nullptr;

static size_t NameStringOffset = 0x0C;

//---------------------------------------------------------------------------
// Validates a candidate wide ASCII string and returns its length. SEH guarded
// so probing an unmapped address can never crash the host process.
//---------------------------------------------------------------------------
static bool SafeProbeWide(const void* address, size_t* outLength)
{
	__try
	{
		const wchar_t* str = reinterpret_cast<const wchar_t*>(address);
		if (str[0] < 0x21 || str[0] > 0x7E)
		{
			return false;
		}
		for (size_t i = 1; i < 256; ++i)
		{
			if (str[i] == 0)
			{
				*outLength = i;
				return true;
			}
			if (str[i] < 0x20 || str[i] > 0x7E)
			{
				return false;
			}
		}
		return false;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

// Finds the offset of the wide name string by testing it against a sample of
// entries until one offset is plausible for every sampled entry.
static void ProbeNameStringOffset()
{
	const size_t candidates[] = { 0x08, 0x0C, 0x10, 0x14, 0x04, 0x18, 0x1C, 0x20 };
	const size_t maxSamples = 48;

	for (size_t c = 0; c < sizeof(candidates) / sizeof(candidates[0]); ++c)
	{
		size_t tested = 0;
		bool consistent = true;

		for (size_t i = 0; i < GlobalNames->Num() && tested < maxSamples; ++i)
		{
			auto entry = (*GlobalNames)[i];
			if (entry == nullptr)
			{
				continue;
			}

			++tested;

			size_t length;
			if (!SafeProbeWide(reinterpret_cast<unsigned char*>(entry) + candidates[c], &length))
			{
				consistent = false;
				break;
			}
		}

		if (consistent && tested > 0)
		{
			NameStringOffset = candidates[c];
			return;
		}
	}
}

bool NamesStore::Initialize()
{
	auto core = GetModuleHandleW(L"Core.dll");
	if (core == nullptr)
	{
		return false;
	}

	GlobalNames = reinterpret_cast<TArray<FNameEntry*>*>(
		GetProcAddress(core, "?Names@FName@@0V?$TArray@PAUFNameEntry@@@@A"));

	if (GlobalNames == nullptr)
	{
		// Ordinal fallback (ordinal 1863 in the analysed Core.dll export table).
		GlobalNames = reinterpret_cast<TArray<FNameEntry*>*>(
			GetProcAddress(core, MAKEINTRESOURCEA(1863)));
	}

	if (GlobalNames == nullptr)
	{
		return false;
	}

	ProbeNameStringOffset();

	return true;
}

void* NamesStore::GetAddress()
{
	return GlobalNames;
}

size_t NamesStore::GetNamesNum() const
{
	return GlobalNames->Num();
}

bool NamesStore::IsValid(size_t id) const
{
	return GlobalNames->IsValidIndex(id) && (*GlobalNames)[id] != nullptr;
}

std::string NamesStore::GetById(size_t id) const
{
	auto entry = (*GlobalNames)[id];
	if (entry == nullptr)
	{
		return std::string();
	}

	auto str = reinterpret_cast<const wchar_t*>(
		reinterpret_cast<unsigned char*>(entry) + NameStringOffset);

	size_t length;
	if (!SafeProbeWide(str, &length))
	{
		return std::string();
	}

	const int size = WideCharToMultiByte(CP_UTF8, 0, str, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, str, static_cast<int>(length), &result[0], size, nullptr, nullptr);
	return result;
}
