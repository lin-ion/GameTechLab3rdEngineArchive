#pragma once

class UEngineStatics
{
public:
	static uint32 GetUUID()
	{
		return NextUUID++;
	}

public:
	static uint32 NextUUID;
};

//일단 단순 디스크립터
class UClass
{
public:
	const char* ClassName;
	uint64		ClassSize;
	UClass*		SuperClass;
};

#include "Memory.h"


