#include "OcclusionManager.h"
#include <algorithm>
#include <iostream>
#include <cmath>

#include "Render/Pipeline/ViewContext.h"
#include "Render/Pipeline/PrimitiveProxy.h"
#include "Component/PrimitiveComponent.h"
#include "Core/EngineTypes.h"
#include "Object/Object.h"

FOcclusionManager& FOcclusionManager::Get()
{
	static FOcclusionManager Instance;
	return Instance;
}

void FOcclusionManager::Initialize(ID3D11Device* InDevice)
{
	if (!HZBBuildCS.Create(InDevice, L"Shaders/HZBBuild.hlsl", "CSMain"))
	{
		std::cerr << "Failed to create HZBBuild CS" << std::endl;
	}

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = (sizeof(uint32) * 4 + 15) & ~15; 
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	InDevice->CreateBuffer(&cbDesc, nullptr, &HZBConstantBuffer);

	// Phase 2
	if (!OcclusionTestCS.Create(InDevice, L"Shaders/OcclusionTest.hlsl", "CSMain"))
	{
		std::cerr << "Failed to create OcclusionTest CS" << std::endl;
	}

	D3D11_BUFFER_DESC otCbDesc = {};
	otCbDesc.ByteWidth = (sizeof(FMatrix) + 16 + 16 + 15) & ~15;
	otCbDesc.Usage = D3D11_USAGE_DYNAMIC;
	otCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	otCbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	InDevice->CreateBuffer(&otCbDesc, nullptr, &OcclusionTestCB);

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	InDevice->CreateSamplerState(&sampDesc, &PointClampSampler);
}

void FOcclusionManager::Release()
{
	HZBBuildCS.Release();
	if (HZBConstantBuffer) { HZBConstantBuffer->Release(); HZBConstantBuffer = nullptr; }
	
	if (HZBSRV) { HZBSRV->Release(); HZBSRV = nullptr; }
	for (auto UAV : HZBMipsUAV) { if (UAV) UAV->Release(); }
	HZBMipsUAV.clear();
	for (auto SRV : HZBMipsSRV) { if (SRV) SRV->Release(); }
	HZBMipsSRV.clear();
	if (HZBTexture) { HZBTexture->Release(); HZBTexture = nullptr; }
	if (PointClampSampler) { PointClampSampler->Release(); PointClampSampler = nullptr; }
	
	HZBWidth = 0;
	HZBHeight = 0;
	HZBMipCount = 0;

	// Phase 2
	OcclusionTestCS.Release();
	if (OcclusionTestCB) { OcclusionTestCB->Release(); OcclusionTestCB = nullptr; }
	if (ProxyBuffer) { ProxyBuffer->Release(); ProxyBuffer = nullptr; }
	if (ProxySRV) { ProxySRV->Release(); ProxySRV = nullptr; }
	if (VisibilityBuffer) { VisibilityBuffer->Release(); VisibilityBuffer = nullptr; }
	if (VisibilityUAV) { VisibilityUAV->Release(); VisibilityUAV = nullptr; }
	if (ReadbackBuffers[0]) { ReadbackBuffers[0]->Release(); ReadbackBuffers[0] = nullptr; }
	if (ReadbackBuffers[1]) { ReadbackBuffers[1]->Release(); ReadbackBuffers[1] = nullptr; }
	ProxyBufferCapacity = 0;
}

void FOcclusionManager::CreateHZBTexture(ID3D11Device* InDevice, uint32 Width, uint32 Height)
{
	uint32 targetWidth = std::max(1u, Width / 2);
	uint32 targetHeight = std::max(1u, Height / 2);

	if (HZBWidth == targetWidth && HZBHeight == targetHeight && HZBTexture)
		return;

	if (HZBSRV) { HZBSRV->Release(); HZBSRV = nullptr; }
	for (auto UAV : HZBMipsUAV) { if (UAV) UAV->Release(); }
	HZBMipsUAV.clear();
	for (auto SRV : HZBMipsSRV) { if (SRV) SRV->Release(); }
	HZBMipsSRV.clear();
	if (HZBTexture) { HZBTexture->Release(); HZBTexture = nullptr; }

	HZBWidth = targetWidth;
	HZBHeight = targetHeight;

	HZBMipCount = static_cast<uint32>(std::floor(std::log2(std::max(HZBWidth, HZBHeight)))) + 1;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = HZBWidth;
	texDesc.Height = HZBHeight;
	texDesc.MipLevels = HZBMipCount;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	HRESULT hr = InDevice->CreateTexture2D(&texDesc, nullptr, &HZBTexture);
	if (FAILED(hr)) return;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = HZBMipCount;
	srvDesc.Texture2D.MostDetailedMip = 0;
	InDevice->CreateShaderResourceView(HZBTexture, &srvDesc, &HZBSRV);

	for (uint32 i = 0; i < HZBMipCount; ++i)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = i;
		ID3D11UnorderedAccessView* uav = nullptr;
		InDevice->CreateUnorderedAccessView(HZBTexture, &uavDesc, &uav);
		HZBMipsUAV.push_back(uav);

		D3D11_SHADER_RESOURCE_VIEW_DESC mipSrvDesc = {};
		mipSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		mipSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		mipSrvDesc.Texture2D.MipLevels = 1;
		mipSrvDesc.Texture2D.MostDetailedMip = i;
		ID3D11ShaderResourceView* mipSrv = nullptr;
		InDevice->CreateShaderResourceView(HZBTexture, &mipSrvDesc, &mipSrv);
		HZBMipsSRV.push_back(mipSrv);
	}
}

void FOcclusionManager::BuildHZB(ID3D11DeviceContext* InContext, ID3D11ShaderResourceView* InDepthSRV, uint32 Width, uint32 Height)
{
	if (!InDepthSRV) return;

	ID3D11Device* device = nullptr;
	InContext->GetDevice(&device);
	if (!device) return;
	
	CreateHZBTexture(device, Width, Height);
	device->Release();

	if (!HZBTexture) return;

	HZBBuildCS.Bind(InContext);

	struct HZBConstants
	{
		uint32 SrcRes[2];
		uint32 DstRes[2];
	};

	uint32 currWidth = Width;
	uint32 currHeight = Height;

	for (uint32 i = 0; i < HZBMipCount; ++i)
	{
		uint32 nextWidth = std::max(1u, currWidth / 2);
		uint32 nextHeight = std::max(1u, currHeight / 2);

		HZBConstants consts;
		consts.SrcRes[0] = currWidth;
		consts.SrcRes[1] = currHeight;
		consts.DstRes[0] = nextWidth;
		consts.DstRes[1] = nextHeight;

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (SUCCEEDED(InContext->Map(HZBConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &consts, sizeof(consts));
			InContext->Unmap(HZBConstantBuffer, 0);
		}

		InContext->CSSetConstantBuffers(0, 1, &HZBConstantBuffer);
		
		ID3D11ShaderResourceView* srcSRV = (i == 0) ? InDepthSRV : HZBMipsSRV[i - 1];
		InContext->CSSetShaderResources(0, 1, &srcSRV);
		InContext->CSSetUnorderedAccessViews(0, 1, &HZBMipsUAV[i], nullptr);

		uint32 groupX = (nextWidth + 7) / 8;
		uint32 groupY = (nextHeight + 7) / 8;
		HZBBuildCS.Dispatch(InContext, groupX, groupY, 1);

		ID3D11UnorderedAccessView* nullUAV = nullptr;
		InContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		ID3D11ShaderResourceView* nullSRV = nullptr;
		InContext->CSSetShaderResources(0, 1, &nullSRV);

		currWidth = nextWidth;
		currHeight = nextHeight;
		
		if (currWidth == 1 && currHeight == 1 && i > 0) break; 
	}

	HZBBuildCS.Unbind(InContext);
}

void FOcclusionManager::UpdateGPUProxies(ID3D11DeviceContext* InContext, const TArray<FPrimitiveProxy*>& InProxies)
{
	if (InProxies.empty()) return;
    
	ID3D11Device* device = nullptr;
	InContext->GetDevice(&device);

	if (ProxyBufferCapacity < (uint32)InProxies.size())
	{
		if (ProxyBuffer) ProxyBuffer->Release();
		if (ProxySRV) ProxySRV->Release();
		if (VisibilityBuffer) VisibilityBuffer->Release();
		if (VisibilityUAV) VisibilityUAV->Release();
		if (ReadbackBuffers[0]) ReadbackBuffers[0]->Release();
		if (ReadbackBuffers[1]) ReadbackBuffers[1]->Release();

		ProxyBufferCapacity = (uint32)InProxies.size() + 64;

		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.ByteWidth = sizeof(FProxyAABB) * ProxyBufferCapacity;
		bufDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufDesc.StructureByteStride = sizeof(FProxyAABB);
		device->CreateBuffer(&bufDesc, nullptr, &ProxyBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = ProxyBufferCapacity;
		device->CreateShaderResourceView(ProxyBuffer, &srvDesc, &ProxySRV);

		D3D11_BUFFER_DESC visDesc = {};
		visDesc.ByteWidth = sizeof(uint32) * ProxyBufferCapacity;
		visDesc.Usage = D3D11_USAGE_DEFAULT;
		visDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		visDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		visDesc.StructureByteStride = sizeof(uint32);
		device->CreateBuffer(&visDesc, nullptr, &VisibilityBuffer);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = ProxyBufferCapacity;
		device->CreateUnorderedAccessView(VisibilityBuffer, &uavDesc, &VisibilityUAV);

		D3D11_BUFFER_DESC rbDesc = {};
		rbDesc.ByteWidth = sizeof(uint32) * ProxyBufferCapacity;
		rbDesc.Usage = D3D11_USAGE_STAGING;
		rbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		device->CreateBuffer(&rbDesc, nullptr, &ReadbackBuffers[0]);
		device->CreateBuffer(&rbDesc, nullptr, &ReadbackBuffers[1]);
		
		bReadbackReady[0] = false;
		bReadbackReady[1] = false;
	}

	if (device) device->Release();

	TArray<FProxyAABB> gpuData;
	gpuData.reserve(InProxies.size());
	ReadbackProxyIds[ReadbackIndex].clear();
	for (auto proxy : InProxies)
	{
		FProxyAABB data;
		FBoundingBox box = proxy->GetAABB();
		data.Min = box.Min;
		data.Max = box.Max;
		data.Id = proxy->GetId();
		gpuData.push_back(data);
		ReadbackProxyIds[ReadbackIndex].push_back(data.Id);
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(InContext->Map(ProxyBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, gpuData.data(), sizeof(FProxyAABB) * gpuData.size());
		InContext->Unmap(ProxyBuffer, 0);
	}
}

void FOcclusionManager::ExecuteOcclusionTest(ID3D11DeviceContext* InContext, const FViewContext& InView, const TArray<FPrimitiveProxy*>& InProxies)
{
	if (InProxies.empty() || !HZBTexture) return;

	// Process readback from PREVIOUS frame (N-1 or N-2)
	uint32 prevIndex = (ReadbackIndex + 1) % 2;
	if (bReadbackReady[prevIndex])
	{
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (SUCCEEDED(InContext->Map(ReadbackBuffers[prevIndex], 0, D3D11_MAP_READ, 0, &mapped)))
		{
			uint32* data = static_cast<uint32*>(mapped.pData);
			const auto& ids = ReadbackProxyIds[prevIndex];
			for (uint32 i = 0; i < ids.size(); ++i)
			{
				VisibilityMap[ids[i]] = (data[i] != 0);
			}
			InContext->Unmap(ReadbackBuffers[prevIndex], 0);
		}
	}

	UpdateGPUProxies(InContext, InProxies);

	OcclusionTestCS.Bind(InContext);

	struct PassConstants
	{
		FMatrix ViewProjection;
		uint32 ProxyCount;
		uint32 HZBMipCount;
		float HZBSize[2];
	};

	PassConstants consts;
	// Use previous frame's ViewProjection if available, otherwise use current
	FMatrix currentVP = (InView.GetView() * InView.GetProj());
	if (bHasPrevViewProjection)
	{
		consts.ViewProjection = PrevViewProjection.GetTransposed();
	}
	else
	{
		consts.ViewProjection = currentVP.GetTransposed();
	}
	
	consts.ProxyCount = (uint32)InProxies.size();
	consts.HZBMipCount = HZBMipCount;
	consts.HZBSize[0] = (float)HZBWidth;
	consts.HZBSize[1] = (float)HZBHeight;

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (SUCCEEDED(InContext->Map(OcclusionTestCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &consts, sizeof(consts));
		InContext->Unmap(OcclusionTestCB, 0);
	}

	InContext->CSSetConstantBuffers(0, 1, &OcclusionTestCB);
	InContext->CSSetShaderResources(0, 1, &HZBSRV);
	InContext->CSSetShaderResources(1, 1, &ProxySRV);
	InContext->CSSetSamplers(0, 1, &PointClampSampler);
	InContext->CSSetUnorderedAccessViews(0, 1, &VisibilityUAV, nullptr);

	uint32 groupX = (consts.ProxyCount + 63) / 64;
	OcclusionTestCS.Dispatch(InContext, groupX, 1, 1);

	ID3D11UnorderedAccessView* nullUAV = nullptr;
	InContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
	InContext->CSSetShaderResources(0, 2, nullSRVs);

	InContext->CopyResource(ReadbackBuffers[ReadbackIndex], VisibilityBuffer);
	bReadbackReady[ReadbackIndex] = true;
	ReadbackIndex = (ReadbackIndex + 1) % 2;

	OcclusionTestCS.Unbind(InContext);

	// Store current ViewProjection for next frame
	PrevViewProjection = currentVP;
	bHasPrevViewProjection = true;
}

bool FOcclusionManager::IsVisible(uint32 ProxyId) const
{
	auto it = VisibilityMap.find(ProxyId);
	if (it != VisibilityMap.end())
	{
		return it->second;
	}
	return true; // Default to visible if no result yet
}
