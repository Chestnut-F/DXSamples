#pragma once
#include "DXSampleHelper.h"

struct LightConstantBuffer
{
	DirectX::XMFLOAT3 Color;
	float Radius;
	DirectX::XMFLOAT3 Position;
	float padding[57];
};
static_assert((sizeof(LightConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

class DXLight
{
public:
	DXLight(DirectX::XMFLOAT3 color, float radius, DirectX::XMFLOAT3 position);
	void UpdateLight(DirectX::XMVECTOR position);
	void UploadLight(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize);
	DirectX::XMVECTOR GetPosition();
private:
	LightConstantBuffer lightConstantBuffer;
	ComPtr<ID3D12Resource> constantBuffer;
	UINT8* pLightCbvDataBegin;
};
