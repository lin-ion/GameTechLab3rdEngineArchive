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
};   