#include "RenderResources.h"

void FRenderResources::Create(ID3D11Device* InDevice)
{
	FrameBuffer.Create(InDevice, sizeof(FFrameConstants));
	PerObjectConstantBuffer.Create(InDevice, sizeof(FPerObjectConstants));

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	InDevice->CreateSamplerState(&sampDesc, &DefaultSampler);
}

#ifdef FOR_COMPETITION
void FRenderResources::CreatePerObjectLargeCB(ID3D11Device* Device, uint32 MaxObjects)
{
	ReleasePerObjectLargeCB();

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth      = MaxObjects * sizeof(FPerObjectAligned);
	desc.Usage           = D3D11_USAGE_DYNAMIC;
	desc.BindFlags       = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;

	Device->CreateBuffer(&desc, nullptr, &PerObjectLargeCB);
	LargeCBCapacity = MaxObjects;
}

void FRenderResources::UpdatePerObjectLargeCB(
    ID3D11DeviceContext* Context, const FPerObjectAligned* Data, uint32 Count)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	Context->Map(PerObjectLargeCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, Data, Count * sizeof(FPerObjectAligned));
	Context->Unmap(PerObjectLargeCB, 0);
}

void FRenderResources::ReleasePerObjectLargeCB()
{
	if (PerObjectLargeCB) { PerObjectLargeCB->Release(); PerObjectLargeCB = nullptr; }
	LargeCBCapacity = 0;
}
#endif // FOR_COMPETITION

void FRenderResources::Release()
{
	FrameBuffer.Release();
	PerObjectConstantBuffer.Release();
#ifdef FOR_COMPETITION
	ReleasePerObjectLargeCB();
#endif
	if (DefaultSampler) { DefaultSampler->Release(); DefaultSampler = nullptr; }
}
