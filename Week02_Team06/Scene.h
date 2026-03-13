#pragma once
class UObject;

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

	UObject* GetSceneObject();

	void Save(const std::string& path);

	void Load(const std::string& path, ID3D11Device* device);


private:
	UObject* TestObject = { nullptr };
};