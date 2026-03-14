#include "pch.h"
#include "Actor.h"
#include "Level.h"
#include "World.h"

UWorld* AActor::GetWorld()
{
	if (OwningLevel)
	{
		return OwningLevel->OwningWorld;
	}
	return nullptr;
}

void AActor::Release()
{
	// 컴포넌트는 NewObject로 생성되어 GUObjectArray가 수명 관리
	// Actor는 참조 목록만 비운다
	Components.Clear();
	RootComponent = nullptr;
}
