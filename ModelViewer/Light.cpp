#include "stdafx.h"
#include "Light.h"

DXLight::DXLight(XMFLOAT3 pos, float radius, XMFLOAT3 color, float area)
{
	lightConstantBuffer.Color = color;
	lightConstantBuffer.PositionW = pos;
	lightConstantBuffer.Radius = radius;
	lightConstantBuffer.Area = area;
}

void DXLight::Update()
{
    memcpy(pLightCbvDataBegin, &lightConstantBuffer, sizeof(LightConstantBuffer));
}

void DXLight::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize)
{
    lightCbvOffset = offsetInHeap;
    const UINT constantBufferSize = sizeof(LightConstantBuffer);    // CB size is required to be 256-byte aligned.

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer)));

    NAME_D3D12_OBJECT(constantBuffer);

    CD3DX12_CPU_DESCRIPTOR_HANDLE lightCbvHandle(cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), lightCbvOffset, cbvSrvDescriptorSize);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize;
    device->CreateConstantBufferView(&cbvDesc, lightCbvHandle);

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pLightCbvDataBegin)));
}

void DXLight::Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvHandle(cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), lightCbvOffset, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable, cbvSrvHandle);
}
