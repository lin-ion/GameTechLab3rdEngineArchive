#pragma once
class UObject;

class UScene
{
public:
	UScene() = default;
	~UScene() = default;

public:
	void Initialize(ID3D11Device& Device);
	void Update(float DeltaTime);
	void Release();

	void AddObject(UObject* object);

	UObject* GetSceneObject();


private:
	UObject* TestObject = { nullptr };
	std::vector<UObject*> SceneObjects;
};   