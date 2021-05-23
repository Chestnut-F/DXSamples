#pragma once
#include "DXSample.h"
#include "Model.h"
#include "Camera.h"
#include "Light.h"
#include "StepTimer.h"

class ModelViewer : public DXSample
{
public:
	ModelViewer(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);
private:
    static const UINT FrameCount = 2;
    static const UINT SSAOKernelSize = 64;
    static const UINT SSAONoiseDim = 8;
    static const UINT SSAONoiseDim2 = SSAONoiseDim * SSAONoiseDim;
    struct SSAOKernelConstantBuffer
    {
        XMFLOAT4 ssaoKernel[SSAOKernelSize];
        XMFLOAT4 ssaoNoise[SSAONoiseDim2];
    }m_ssaoKernel;
    static_assert((sizeof(SSAOKernelConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

    std::unique_ptr<DXModel> m_pModel;
    std::unique_ptr<DXCamera> m_pCamera;
    std::unique_ptr<DXLight> m_pLight;

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap;
    ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
    UINT m_cbvSrvDescriptorSize;

    // Intermediate render targets.
    ComPtr<ID3D12RootSignature> m_gbufferRootSignature;
    ComPtr<ID3D12PipelineState> m_gbufferState;
    ComPtr<ID3D12Resource> m_positionDepthRenderTarget;
    ComPtr<ID3D12Resource> m_normalRenderTarget;
    ComPtr<ID3D12Resource> m_albedoRenderTarget;
    ComPtr<ID3D12RootSignature> m_ssaoRootSignature;
    ComPtr<ID3D12PipelineState> m_ssaoState;
    UINT m_ssaoCbvOffset;
    UINT8* m_pSSAOCbvDataBegin;
    ComPtr<ID3D12Resource> m_ssaoConstantBuffer;
    ComPtr<ID3D12Resource> m_ssaoRenderTarget;
    ComPtr<ID3D12PipelineState> m_blurState;

    // Render targets.
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthStencil;
    UINT m_rtvDescriptorSize;

    // App resources.
    UINT m_frameCounter;
    StepTimer m_timer;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    void InitDevice();
    void CreateDescriptorHeaps();
    void CreateRootSignature();
    void CreatePipelineState();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
};
