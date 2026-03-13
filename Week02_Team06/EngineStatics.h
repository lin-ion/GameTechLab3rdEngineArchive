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

