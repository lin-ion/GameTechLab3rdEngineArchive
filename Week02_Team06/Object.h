#pragma once

class UMesh;

class UObject
{
public:
	UObject() = default;
	~UObject() = default;

public:
	FVector& GetPosition() { return Position; };
	float	 GetRadius() { return Radius;  }
public:
	void Release();

	void Update(float DeltaTime);
	void Render(ID3D11DeviceContext& DeviceContext);

	//추후 컴포넌트로 수정
	void AddMesh(ID3D11Device& Device, const FVertexSimple* vertices, UINT vertexCount);

private:

	//나중에 컴포넌트로 바꿀 예정
	//추후 Matrix로 스케일 / 회전 / 위치 정보를 저장
	FVector Position;
	float Radius = { 0.1f };

	//VertexBuffer를 소유
	UMesh* Mesh;

};

