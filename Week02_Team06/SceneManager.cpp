#include "pch.h"
#include "SceneManager.h"
#include "Scene.h"

USceneManager::USceneManager()
{
}

USceneManager::~USceneManager()
{
}

void USceneManager::Initialize(ID3D11Device& Device)
{
	if (Scene)
	{
		Scene->Release();
	}

	//새로운 씬 생성
	Scene = new UScene();
	Scene->Initialize(Device);
}

void USceneManager::Update(float DeltaTime)
{
	if (Scene)
	{
		Scene->Update(DeltaTime);
	}
}

void USceneManager::Release()
{
	if (Scene)
	{
		Scene->Release();
		delete Scene;
	}
}
