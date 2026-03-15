#pragma once
class UObject;
class UCameraComponent;

struct SceneSaveData
{
	std::string Name;
	int Version;
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
	// TODO: Scene이 바뀌어도 MainCamera는 유지되어야 하므로 다른 곳에서 관리하도록 수정 고려
	UCameraComponent* MainCamera;
	SceneSaveData data;
};