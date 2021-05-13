#pragma once
#include "DXSampleHelper.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct LightConstantBuffer
{
    XMFLOAT3 Color;
    float Radius;
    XMFLOAT3 PositionW;
    float Area;
    float padding[56];
};
static_assert((sizeof(LightConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

class DXLight
{
public:
    DXLight(XMFLOAT3 pos, float radius, XMFLOAT3 color = { 1.0f, 1.0f, 1.0f }, float area = 1.0f);

    void Update();
    void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize);
    void Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap,
        INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize);

private:
    LightConstantBuffer lightConstantBuffer;
    UINT lightCbvOffset;
    UINT8* pLightCbvDataBegin;
    ComPtr<ID3D12Resource> constantBuffer;
};