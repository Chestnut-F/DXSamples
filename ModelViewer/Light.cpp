#include "stdafx.h"
#include "Light.h"

DXLight::DXLight(DirectX::XMFLOAT3 color, float radius, DirectX::XMFLOAT3 position)
{
    lightConstantBuffer.Color = color;
    lightConstantBuffer.Radius = radius;
    lightConstantBuffer.Position = position;
}

void DXLight::UpdateLight(DirectX::XMVECTOR position)
{
    DirectX::XMStoreFloat3(&lightConstantBuffer.Position, position);
    memcpy(pLightCbvDataBegin, &lightConstantBuffer, sizeof(LightConstantBuffer));
}

void DXLight::UploadLight(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize)
{
    const UINT constantBufferSize = sizeof(LightConstantBuffer);    // CB size is required to be 256-byte aligned.

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constantBuffer)));

    NAME_D3D12_OBJECT(constantBuffer);

    CD3DX12_CPU_DESCRIPTOR_HANDLE transformCbvHandle(cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), offsetInHeap, cbvSrvDescriptorSize);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize;
    device->CreateConstantBufferView(&cbvDesc, transformCbvHandle);

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pLightCbvDataBegin)));
}

DirectX::XMVECTOR DXLight::GetPosition()
{
    return DirectX::XMLoadFloat3(&lightConstantBuffer.Position);
}
