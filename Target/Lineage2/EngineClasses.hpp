#pragma once

// Lineage 2 (Unreal Engine 2, license 2110) engine class layouts.
//
// The native class sizes below were taken from a live GObjObjects snapshot of
// the "Light of Elcardia" client (Core.dll, build 746):
//
//   Object 0x34   Field 0x40   Struct 0x8C   State 0x4A4   Class 0x52C
//   Const 0x4C    Enum 0x50    Property 0x80 Function 0xB0
//   RefLinkProperty 0x84  ObjectProperty 0x88  ClassProperty 0x8C
//
// UObject is fully pinned by the snapshot (Outer 0x18, ObjectFlags 0x1C,
// Name 0x20, Class 0x24). The internal offsets of UStruct/UField/UProperty
// that the generator walks (Children, Next, SuperField, Offset, PropertySize)
// are refined at runtime by Calibration.cpp; the values declared here are the
// canonical UE2 defaults used when calibration cannot run.

#include <cstdint>
#include <set>
#include <string>
#include <windows.h>

struct FPointer
{
	uintptr_t Dummy;
};

struct FQWord
{
	int32_t A;
	int32_t B;
};

struct FName
{
	int32_t Index;
};

template<class T>
struct TArray
{
	friend struct FString;

public:
	TArray()
	{
		Data = nullptr;
		Count = Max = 0;
	}

	size_t Num() const
	{
		return Count;
	}

	T& operator[](size_t i)
	{
		return Data[i];
	}

	const T& operator[](size_t i) const
	{
		return Data[i];
	}

	bool IsValidIndex(size_t i) const
	{
		return i < Num();
	}

private:
	T* Data;
	int32_t Count;
	int32_t Max;
};

struct FString : public TArray<wchar_t>
{
	std::string ToString() const
	{
		if (Data == nullptr || Count <= 1)
		{
			return std::string();
		}

		const int size = WideCharToMultiByte(CP_UTF8, 0, Data, Count - 1, nullptr, 0, nullptr, nullptr);
		std::string str(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, Data, Count - 1, &str[0], size, nullptr, nullptr);
		return str;
	}
};

// Lineage 2 FScriptDelegate is 8 bytes (object index + function name).
struct FScriptDelegate
{
	unsigned char UnknownData[0x08];
};

class UClass;

//---------------------------------------------------------------------------
// UObject - 0x34. Layout confirmed by the GObjObjects snapshot.
//---------------------------------------------------------------------------
class UObject
{
public:
	void*			VfTableObject;			// 0x00
	int32_t			InternalIndex;			// 0x04  index inside GObjObjects
	unsigned char	UnknownData00[0x10];	// 0x08
	UObject*		Outer;					// 0x18
	int32_t			ObjectFlags;			// 0x1C
	FName			Name;					// 0x20
	UClass*			Class;					// 0x24
	unsigned char	UnknownData01[0x0C];	// 0x28
};

//---------------------------------------------------------------------------
// UField - 0x40
//---------------------------------------------------------------------------
class UField : public UObject
{
public:
	UField*			SuperField;				// 0x34
	UField*			Next;					// 0x38
	UField*			HashNext;				// 0x3C
};

//---------------------------------------------------------------------------
// UEnum - 0x50
//---------------------------------------------------------------------------
class UEnum : public UField
{
public:
	TArray<FName>	Names;					// 0x40
	unsigned char	UnknownData00[0x04];	// 0x4C
};

//---------------------------------------------------------------------------
// UConst - 0x4C
//---------------------------------------------------------------------------
class UConst : public UField
{
public:
	FString			Value;					// 0x40
};

//---------------------------------------------------------------------------
// UStruct - 0x8C
//---------------------------------------------------------------------------
class UStruct : public UField
{
public:
	unsigned char	UnknownData00[0x08];	// 0x40
	UField*			Children;				// 0x48
	uint32_t		PropertySize;			// 0x4C
	unsigned char	UnknownData01[0x3C];	// 0x50
};

//---------------------------------------------------------------------------
// UFunction - 0xB0
//---------------------------------------------------------------------------
class UFunction : public UStruct
{
public:
	uint32_t		FunctionFlags;			// 0x8C
	uint16_t		iNative;				// 0x90
	uint16_t		RepOffset;				// 0x92
	uint8_t			OperPrecedence;			// 0x94
	uint8_t			NumParms;				// 0x95
	uint16_t		ParmsSize;				// 0x96
	uint32_t		ReturnValueOffset;		// 0x98
	unsigned char	UnknownData00[0x10];	// 0x9C
	void*			Func;					// 0xAC
};

//---------------------------------------------------------------------------
// UState - 0x4A4
//---------------------------------------------------------------------------
class UState : public UStruct
{
public:
	unsigned char	UnknownData00[0x418];	// 0x8C
};

//---------------------------------------------------------------------------
// UClass - 0x52C
//---------------------------------------------------------------------------
class UClass : public UState
{
public:
	unsigned char	UnknownData00[0x88];	// 0x4A4
};

//---------------------------------------------------------------------------
// UProperty - 0x80
//---------------------------------------------------------------------------
class UProperty : public UField
{
public:
	uint32_t		ArrayDim;				// 0x40
	uint32_t		ElementSize;			// 0x44
	uint32_t		PropertyFlags;			// 0x48
	unsigned char	UnknownData00[0x08];	// 0x4C
	uint32_t		Offset;					// 0x54
	unsigned char	UnknownData01[0x28];	// 0x58
};

// Lineage 2 routes every reference-holding property through URefLinkProperty.
class URefLinkProperty : public UProperty
{
public:
	UProperty*		NextRef;				// 0x80
};

// Core.PtrProperty - raw pointer member.
class UPointerProperty : public URefLinkProperty
{
};

class UByteProperty : public UProperty
{
public:
	UEnum*			Enum;					// 0x80
};

class UIntProperty : public UProperty
{
};

class UFloatProperty : public UProperty
{
};

class UBoolProperty : public UProperty
{
public:
	uint32_t		BitMask;				// 0x80
};

class UObjectProperty : public URefLinkProperty
{
public:
	UClass*			PropertyClass;			// 0x84
};

class UClassProperty : public UObjectProperty
{
public:
	UClass*			MetaClass;				// 0x88
};

// Lineage 2 has no Core.InterfaceProperty; kept for interface compatibility.
class UInterfaceProperty : public UProperty
{
public:
	UClass*			InterfaceClass;
};

class UNameProperty : public URefLinkProperty
{
};

class UStructProperty : public URefLinkProperty
{
public:
	UStruct*		Struct;					// 0x84
};

class UStrProperty : public UProperty
{
};

class UArrayProperty : public URefLinkProperty
{
public:
	UProperty*		Inner;					// 0x84
};

class UMapProperty : public UProperty
{
public:
	UProperty*		KeyProp;				// 0x80
	UProperty*		ValueProp;				// 0x84
};

class UDelegateProperty : public URefLinkProperty
{
public:
	UFunction*		SignatureFunction;		// 0x84
};

// Core.FixedArrayProperty - body 8 bytes (Inner + Count).
class UFixedArrayProperty : public UProperty
{
public:
	UProperty*		Inner;					// 0x80
	uint32_t		Count;					// 0x84
};
