#pragma once
#include "DXSampleHelper.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static const UINT SSAOKernelSize = 64;
static const UINT SSAONoiseWidth = 1280;
static const UINT SSAONoiseHeight = 720;

struct SSAOKernelConstantBuffer
{
    XMFLOAT4 ssaoKernel[SSAOKernelSize];
};
static_assert((sizeof(SSAOKernelConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct SSAONoiseConstantBuffer
{
    XMFLOAT4 ssaoNoise[SSAONoiseWidth * SSAONoiseHeight];
};
static_assert((sizeof(SSAOKernelConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

class SSAO
{
public:
    SSAO();

    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device, const std::wstring& vsName, 
        const std::wstring& psName, const std::wstring& psBlurName);

    INT Init(ID3D12Device* device, UINT width, UINT height, 
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, UINT rtvDescriptorSize);
    void Update();
    void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize);
    void Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
        INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize, CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle);
    void Blur(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap,
        INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize);

    INT ssaoCbvSrvOffset;
    INT ssaoCbvSrvOffsetEnd;

    ID3D12RootSignature* GetRootSignature() { return ssaoRootSignature.Get(); }
    ID3D12RootSignature* GetBlurRootSignature() { return ssaoBlurRootSignature.Get(); }
    ID3D12Resource* GetRenderTarget() { return ssaoRenderTarget.Get(); }
    ID3D12Resource* GetBlurRenderTarget() { return ssaoBlurRenderTarget.Get(); }

private:
    ComPtr<ID3D12RootSignature> ssaoRootSignature;
    ComPtr<ID3D12PipelineState> ssaoState;
    ComPtr<ID3D12RootSignature> ssaoBlurRootSignature;
    ComPtr<ID3D12PipelineState> ssaoBlurState;

    CD3DX12_CPU_DESCRIPTOR_HANDLE ssaoRtvHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE ssaoBlurRtvHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE ssaoSrvHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE ssaoBlurSrvHandle;
    ComPtr<ID3D12Resource> ssaoRenderTarget;
    ComPtr<ID3D12Resource> ssaoBlurRenderTarget;

    UINT8* pSSAOCbvDataBegin;
    ComPtr<ID3D12Resource> ssaoConstantBuffer;
    SSAOKernelConstantBuffer ssaoKernel;
    ComPtr<ID3D12Resource> ssaoNoiseTexture;
    ComPtr<ID3D12Resource> ssaoNoiseTextureUploadHeap;
    SSAONoiseConstantBuffer ssaoNoise;
};