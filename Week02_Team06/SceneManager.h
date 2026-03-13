#pragma once
class UScene;
class UObject;

class USceneManager
{

public:
	USceneManager();
	~USceneManager();

public:
	UScene* GetCurrentScene() const { return Scene; };
	void Initialize(ID3D11Device& Device);
	void Update(float DeltaTime);
	void Release();

private:
	UScene* Scene = { nullptr };
};