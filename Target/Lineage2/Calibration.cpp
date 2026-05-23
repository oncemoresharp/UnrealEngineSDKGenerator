#include <windows.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ObjectsStore.hpp"
#include "NamesStore.hpp"

#include "EngineClasses.hpp"
#include "Calibration.hpp"

namespace L2
{
	size_t Off_UField_SuperField		= 0x34;
	size_t Off_UField_Next				= 0x38;
	size_t Off_UStruct_Children			= 0x48;
	size_t Off_UStruct_PropertySize		= 0x4C;
	size_t Off_UProperty_Offset			= 0x54;
}

// Offset of UObject::Name - pinned by the GObjObjects snapshot, never probed.
static const size_t kNameOffset = 0x20;

//---------------------------------------------------------------------------
// Structured-exception guarded reads. Kept free of C++ objects so the
// __try/__except blocks never collide with stack unwinding.
//---------------------------------------------------------------------------
static bool SafeReadPtr(const void* address, void** out)
{
	__try
	{
		*out = *reinterpret_cast<void* const*>(address);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

static bool SafeReadU32(const void* address, uint32_t* out)
{
	__try
	{
		*out = *reinterpret_cast<const uint32_t*>(address);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

//---------------------------------------------------------------------------
static void Report(const std::string& line)
{
	OutputDebugStringA(("[Lineage2 SDK] " + line + "\n").c_str());
}

static std::string Hex(size_t value)
{
	char buffer[16];
	wsprintfA(buffer, "0x%X", static_cast<unsigned int>(value));
	return std::string(buffer);
}

// Reads the name of a candidate object using the fixed UObject::Name offset.
static std::string ObjName(const void* object)
{
	if (object == nullptr)
	{
		return std::string();
	}

	uint32_t index;
	if (!SafeReadU32(reinterpret_cast<const uint8_t*>(object) + kNameOffset, &index))
	{
		return std::string();
	}

	NamesStore names;
	if (!names.IsValid(index))
	{
		return std::string();
	}

	return names.GetById(index);
}

// Walks a Children/Next style linked list. Returns false if the chain hits an
// unreadable node, loops, or fails to terminate - i.e. the offsets are wrong.
static bool WalkChain(const void* head, size_t nextOffset, std::vector<void*>& nodes)
{
	nodes.clear();

	std::set<const void*> seen;
	const void* current = head;

	for (int step = 0; step < 8192 && current != nullptr; ++step)
	{
		if (!seen.insert(current).second)
		{
			return false;
		}

		if (ObjName(current).empty())
		{
			return false;
		}

		nodes.push_back(const_cast<void*>(current));

		void* next;
		if (!SafeReadPtr(reinterpret_cast<const uint8_t*>(current) + nextOffset, &next))
		{
			return false;
		}

		current = next;
	}

	return current == nullptr && !nodes.empty();
}

namespace L2
{
	bool CalibrateOffsets()
	{
		// The eight native properties declared on Core.Object, with the byte
		// offsets confirmed by the GObjObjects snapshot.
		const std::set<std::string> objectProperties = {
			"ObjectInternal", "Outer", "ObjectFlags", "Name",
			"Class", "CacheIndex", "HashNextBuffer", "IndexBuffer"
		};

		void* objectClass = ObjectsStore().FindClass("Class Core.Object").GetAddress();
		void* fieldClass = ObjectsStore().FindClass("Class Core.Field").GetAddress();

		if (objectClass == nullptr || fieldClass == nullptr)
		{
			Report("Calibration aborted: could not locate Core.Object / Core.Field.");
			return false;
		}

		bool layoutFound = false;

		// Cross-validate Children, Next and Offset together: only the correct
		// triple makes the Name/Class/Outer properties report their known
		// offsets (0x20 / 0x24 / 0x18).
		for (size_t childOffset = 0x40; childOffset <= 0x80 && !layoutFound; childOffset += 4)
		{
			// A UClass children list mixes properties, functions, structs and
			// enums, so the head only has to be a readable named object - the
			// chain walk below decides whether this really is Children.
			void* firstChild;
			if (!SafeReadPtr(reinterpret_cast<uint8_t*>(objectClass) + childOffset, &firstChild))
			{
				continue;
			}
			if (firstChild == nullptr || ObjName(firstChild).empty())
			{
				continue;
			}

			for (size_t nextOffset = 0x34; nextOffset <= 0x3C && !layoutFound; nextOffset += 4)
			{
				std::vector<void*> nodes;
				if (!WalkChain(firstChild, nextOffset, nodes))
				{
					continue;
				}

				std::map<std::string, void*> properties;
				for (void* node : nodes)
				{
					const std::string name = ObjName(node);
					if (objectProperties.find(name) != objectProperties.end())
					{
						properties[name] = node;
					}
				}

				if (properties.size() != objectProperties.size())
				{
					continue;
				}

				void* nameProp = properties["Name"];
				void* classProp = properties["Class"];
				void* outerProp = properties["Outer"];

				for (size_t offsetOffset = 0x40; offsetOffset <= 0x80; offsetOffset += 4)
				{
					uint32_t nameValue, classValue, outerValue;
					if (!SafeReadU32(reinterpret_cast<uint8_t*>(nameProp) + offsetOffset, &nameValue)
						|| !SafeReadU32(reinterpret_cast<uint8_t*>(classProp) + offsetOffset, &classValue)
						|| !SafeReadU32(reinterpret_cast<uint8_t*>(outerProp) + offsetOffset, &outerValue))
					{
						continue;
					}

					if (nameValue == 0x20 && classValue == 0x24 && outerValue == 0x18)
					{
						Off_UStruct_Children = childOffset;
						Off_UField_Next = nextOffset;
						Off_UProperty_Offset = offsetOffset;
						layoutFound = true;
						break;
					}
				}
			}
		}

		if (layoutFound)
		{
			Report("Confirmed UStruct::Children = " + Hex(Off_UStruct_Children)
				+ ", UField::Next = " + Hex(Off_UField_Next)
				+ ", UProperty::Offset = " + Hex(Off_UProperty_Offset) + ".");
		}
		else
		{
			Report("WARNING: could not confirm Children/Next/Offset; using UE2 defaults.");
		}

		// UField::SuperField - Core.Field derives from Core.Object.
		bool superFound = false;
		for (size_t superOffset = 0x34; superOffset <= 0x3C; superOffset += 4)
		{
			if (superOffset == Off_UField_Next)
			{
				continue;
			}

			void* super;
			if (SafeReadPtr(reinterpret_cast<uint8_t*>(fieldClass) + superOffset, &super)
				&& ObjName(super) == "Object")
			{
				Off_UField_SuperField = superOffset;
				superFound = true;
				break;
			}
		}

		if (superFound)
		{
			Report("Confirmed UField::SuperField = " + Hex(Off_UField_SuperField) + ".");
		}
		else
		{
			Report("WARNING: could not confirm SuperField; using UE2 default 0x34.");
		}

		// UStruct::PropertySize - Object instances are 52 bytes, Field 64.
		bool sizeFound = false;
		for (size_t sizeOffset = 0x40; sizeOffset <= 0x80; sizeOffset += 4)
		{
			uint32_t objectSize, fieldSize;
			if (SafeReadU32(reinterpret_cast<uint8_t*>(objectClass) + sizeOffset, &objectSize)
				&& SafeReadU32(reinterpret_cast<uint8_t*>(fieldClass) + sizeOffset, &fieldSize)
				&& objectSize == 52 && fieldSize == 64)
			{
				Off_UStruct_PropertySize = sizeOffset;
				sizeFound = true;
				break;
			}
		}

		if (sizeFound)
		{
			Report("Confirmed UStruct::PropertySize = " + Hex(Off_UStruct_PropertySize) + ".");
		}
		else
		{
			Report("WARNING: could not confirm PropertySize; using UE2 default 0x4C.");
		}

		return layoutFound && superFound && sizeFound;
	}
}
