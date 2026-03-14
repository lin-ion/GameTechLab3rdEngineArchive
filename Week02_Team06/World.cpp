#include "pch.h"
#include "World.h"

void UWorld::Release()
{
	// Level의 수명도 GUObjectArray가 관리
	// World는 참조만 정리
	PersistentLevel = nullptr;
}
