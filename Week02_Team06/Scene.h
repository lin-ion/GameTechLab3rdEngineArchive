#pragma once
class UObject;
class UCameraComponent;

struct SceneSaveData
{
	std::string Version;
	int NextUUID;
};

class UScene
{
public:
	UScene() = default;
	~UScene() = default;

public:
	void Initialize(ID3D11Device& Device);
	void Update(float DeltaTime);
	void Release();

public:
	UCameraComponent* MainCamera;
};