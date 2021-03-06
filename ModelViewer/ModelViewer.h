#pragma once
#include "DXSample.h"
#include "Model.h"
#include "Camera.h"
#include "Light.h"
#include "SSAO.h"
#include "ImageBasedLighting.h"
#include "StepTimer.h"

//#define SSAO_ON

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

    std::unique_ptr<DXModel> m_pModel;
    std::unique_ptr<DXCamera> m_pCamera;
    std::unique_ptr<DXLight> m_pLight;
    std::unique_ptr<DXImageBasedLighting> m_pIBL;

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;
    ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
    UINT m_cbvSrvDescriptorSize;

    // Skybox render targets.
    ComPtr<ID3D12RootSignature> m_skyboxRootSignature;
    ComPtr<ID3D12PipelineState> m_skyboxState;

    // Intermediate render targets.
    ComPtr<ID3D12RootSignature> m_gbufferRootSignature;
    ComPtr<ID3D12PipelineState> m_gbufferState;
    ComPtr<ID3D12Resource> m_positionDepthRenderTarget;
    ComPtr<ID3D12Resource> m_normalRenderTarget;
    ComPtr<ID3D12Resource> m_tangentRenderTarget;
    ComPtr<ID3D12Resource> m_albedoRenderTarget;

#ifdef SSAO_ON
    // SSAO
    std::unique_ptr<SSAO> m_pSSAO;
#endif // SSAO_ON

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
    void PreCompute();
    void PopulateCommandList();
    void WaitForPreviousFrame();
};
