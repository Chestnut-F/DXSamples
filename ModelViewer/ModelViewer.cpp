#include "stdafx.h"
#include "ModelViewer.h"

ModelViewer::ModelViewer(UINT width, UINT height, std::wstring name):
	DXSample(width, height, name),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_frameCounter(0)
{
}

void ModelViewer::OnInit()
{
    m_pCamera = std::make_unique<DXCamera>(XMVECTOR({ 2.0f, 0.0f, -2.0f }), XMVECTOR({ -1.0f, 0.0f, 1.0f }), XMVECTOR({ 0.0f, -1.0f, 0.0f }));
    m_pLight = std::make_unique<DXLight>(XMFLOAT3({ 0.0f, 100.0f, 0.0f }), 2000.0f);
#ifdef SSAO_ON
    m_pSSAO = std::make_unique<SSAO>();
#endif // SSAO_ON
    m_pIBL = std::make_unique<DXImageBasedLighting>();

    InitDevice();
    CreateDescriptorHeaps();
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
    CreateRootSignature();
    CreatePipelineState();
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_gbufferState.Get(), IID_PPV_ARGS(&m_commandList)));
    LoadAssets();

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    WaitForPreviousFrame();

    // Precomputation.
    PreCompute();
}


void ModelViewer::InitDevice()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    NAME_D3D12_OBJECT(m_commandQueue);

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create the fence.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }
}

// Create RTV and DSV heaps.
void ModelViewer::CreateDescriptorHeaps()
{
    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
#ifdef SSAO_ON
    rtvHeapDesc.NumDescriptors =
        FrameCount +                // Frame Count
        4 +                         // G Buffer
        2 +                         // SSAO + SSAO Blur
        1;                          // PBR IBL
#else
    rtvHeapDesc.NumDescriptors =
        FrameCount +                // Frame Count
        4;                          // G Buffer
#endif // SSAO_ON
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

    // Create a RTV for each frame.
    for (UINT n = 0; n < FrameCount; n++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
        m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);

        NAME_D3D12_OBJECT_INDEXED(m_renderTargets, n);
    }
    
    // Create intermediate render targets that are the same dimensions as the swap chain.
    {
        D3D12_RESOURCE_DESC renderTargetDesc;
        renderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        renderTargetDesc.Alignment = 0;
        renderTargetDesc.Width = m_width;
        renderTargetDesc.Height = m_height;
        renderTargetDesc.DepthOrArraySize = 1;
        renderTargetDesc.MipLevels = 1;
        renderTargetDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        renderTargetDesc.SampleDesc.Count = 1;
        renderTargetDesc.SampleDesc.Quality = 0;
        renderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        renderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        D3D12_CLEAR_VALUE clearValue = {};
        memcpy(clearValue.Color, clearColor, sizeof(clearColor));
        clearValue.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        rtvDesc.Texture2D.MipSlice = 0;
        rtvDesc.Texture2D.PlaneSlice = 0;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &renderTargetDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(&m_positionDepthRenderTarget)
        ));
        NAME_D3D12_OBJECT(m_positionDepthRenderTarget);
        m_device->CreateRenderTargetView(m_positionDepthRenderTarget.Get(), &rtvDesc, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);

        renderTargetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &renderTargetDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(&m_normalRenderTarget)
        ));
        NAME_D3D12_OBJECT(m_normalRenderTarget);
        m_device->CreateRenderTargetView(m_normalRenderTarget.Get(), &rtvDesc, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &renderTargetDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(&m_tangentRenderTarget)
        ));
        NAME_D3D12_OBJECT(m_tangentRenderTarget);
        m_device->CreateRenderTargetView(m_tangentRenderTarget.Get(), &rtvDesc, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &renderTargetDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(&m_albedoRenderTarget)
        ));
        NAME_D3D12_OBJECT(m_albedoRenderTarget);
        m_device->CreateRenderTargetView(m_albedoRenderTarget.Get(), &rtvDesc, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }

    INT offset = 0;
#ifdef SSAO_ON
    // Create ssao render targets that are the same dimensions as the swap chain.
    offset = m_pSSAO->Init(m_device.Get(), m_width, m_height, rtvHandle, m_rtvDescriptorSize);
    rtvHandle.Offset(offset, m_rtvDescriptorSize);
#endif // SSAO_ON

    // Describe and create a depth stencil view (DSV) descriptor heap.
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        // Create the depth stencil view.
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&m_depthStencil)
        ));

        NAME_D3D12_OBJECT(m_depthStencil);

        m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Describe and create a sampler descriptor heap.
    {
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.NumDescriptors = 1;
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));

        NAME_D3D12_OBJECT(m_samplerHeap);
    }
}

void ModelViewer::CreateRootSignature()
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // PBR IBL
    m_pIBL->CreateRootSignature(m_device.Get());

    // Skybox pass.
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rootParameters[2];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1]);

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_skyboxRootSignature)));
        NAME_D3D12_OBJECT(m_skyboxRootSignature);
    }

    // G-Buffer Pass.
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[12];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[9].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[10].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[11].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rootParameters[12];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[3].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[4].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[5].InitAsDescriptorTable(1, &ranges[5], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[6].InitAsDescriptorTable(1, &ranges[6], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[7].InitAsDescriptorTable(1, &ranges[7], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[8].InitAsDescriptorTable(1, &ranges[8], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[9].InitAsDescriptorTable(1, &ranges[9], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[10].InitAsDescriptorTable(1, &ranges[10], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[11].InitAsDescriptorTable(1, &ranges[11], D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_STATIC_SAMPLER_DESC defaultSampler(0, D3D12_FILTER_ANISOTROPIC);
        defaultSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_STATIC_SAMPLER_DESC brdflutSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        brdflutSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        brdflutSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        brdflutSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = { defaultSampler,brdflutSampler };

        // Allow input layout and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 2, staticSamplers, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_gbufferRootSignature)));
        NAME_D3D12_OBJECT(m_gbufferRootSignature);
    }

#ifdef SSAO_ON
    // SSAO Pass.
    m_pSSAO->CreateRootSignature(m_device.Get());
#endif // SSAO_ON

    // Final Pass.
    {
#ifdef SSAO_ON
        CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

        CD3DX12_ROOT_PARAMETER1 rootParameters[2];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
#else
        CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

        CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
#endif // SSAO_ON

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Allow input layout and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        NAME_D3D12_OBJECT(m_rootSignature);
    }
}

void ModelViewer::CreatePipelineState()
{
    // SSAO Pass.
    //m_pIBL->CreatePipelineState(m_device.Get(), GetAssetFullPath(L"BRDFLUT.cso"));

    // Skybox Pass.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        ThrowIfFailed(D3DReadFileToBlob(GetAssetFullPath(L"SkyboxVS.cso").c_str(), &vertexShader));
        ThrowIfFailed(D3DReadFileToBlob(GetAssetFullPath(L"SkyboxPS.cso").c_str(), &pixelShader));

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { nullptr, 0 };
        psoDesc.pRootSignature = m_skyboxRootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_skyboxState)));
        NAME_D3D12_OBJECT(m_skyboxState);
    }

    // G-Buffer Pass.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        ThrowIfFailed(D3DReadFileToBlob(GetAssetFullPath(L"PBRVS.cso").c_str(), &vertexShader));
        ThrowIfFailed(D3DReadFileToBlob(GetAssetFullPath(L"PBRPS.cso").c_str(), &pixelShader));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_gbufferRootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 4;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
        psoDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_gbufferState)));
        NAME_D3D12_OBJECT(m_gbufferState);
    }

#ifdef SSAO_ON
    // SSAO Pass.
    m_pSSAO->CreatePipelineState(m_device.Get(), GetAssetFullPath(L"VS.cso"),
        GetAssetFullPath(L"SSAO.cso"), GetAssetFullPath(L"SSAOBlur.cso"));
#endif // SSAO_ON

    // Final Pass.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        ThrowIfFailed(D3DReadFileToBlob(GetAssetFullPath(L"VS.cso").c_str(), &vertexShader));
        ThrowIfFailed(D3DReadFileToBlob(GetAssetFullPath(L"PS.cso").c_str(), &pixelShader));

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { nullptr, 0 };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
        NAME_D3D12_OBJECT(m_pipelineState);
    }
}

void ModelViewer::LoadAssets()
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    //std::wstring ws_assetFullPath = GetAssetFullPath(L"..\\..\\models\\Sponza\\Sponza.gltf");
    std::wstring ws_assetFullPath = GetAssetFullPath(L"..\\..\\models\\DamagedHelmet\\DamagedHelmet.gltf");
    std::string s_assetFullPath(converterX.to_bytes(ws_assetFullPath));
    m_pModel = std::make_unique<DXModel>(s_assetFullPath);

    std::wstring ws_envMapFullPath = GetAssetFullPath(L"..\\..\\textures\\gcanyon_cube.dds");

    D3D12_DESCRIPTOR_HEAP_DESC cbvSrvHeapDesc = {};
#ifdef SSAO_ON
    cbvSrvHeapDesc.NumDescriptors =
        1 +                                 // Camera
        1 +                                 // Light
        m_pModel->primitiveSize +           // Model Primitive
        (m_pModel->materialSize * 6) +      // Model Material
        4 +                                 // G Buffer
        4 +                                 // SSAO
        1;                                  // PBR IBL
#else
    cbvSrvHeapDesc.NumDescriptors =
        1 +                                 // Camera
        1 +                                 // Light
        m_pModel->primitiveSize +           // Model Primitive
        (m_pModel->materialSize * 6) +      // Model Material
        4 +                                 // G Buffer
        20;                                 // PBR IBL
#endif // SSAO_ON
    cbvSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvSrvHeapDesc, IID_PPV_ARGS(&m_cbvSrvUavHeap)));
    NAME_D3D12_OBJECT(m_cbvSrvUavHeap);

    m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    m_pCamera->Upload(m_device.Get(), m_commandList.Get(), m_cbvSrvUavHeap.Get(), 0, m_cbvSrvDescriptorSize);
    m_pLight->Upload(m_device.Get(), m_commandList.Get(), m_cbvSrvUavHeap.Get(), 1, m_cbvSrvDescriptorSize);
    m_pModel->Upload(m_device.Get(), m_commandList.Get(), m_cbvSrvUavHeap.Get(), 2, m_cbvSrvDescriptorSize);

    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
            m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), m_pModel->cbvSrvOffsetEnd, m_cbvSrvDescriptorSize);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_positionDepthRenderTarget.Get(), &srvDesc, srvHandle);
        srvHandle.Offset(1, m_cbvSrvDescriptorSize);

        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        m_device->CreateShaderResourceView(m_normalRenderTarget.Get(), &srvDesc, srvHandle);
        srvHandle.Offset(1, m_cbvSrvDescriptorSize);

        m_device->CreateShaderResourceView(m_tangentRenderTarget.Get(), &srvDesc, srvHandle);
        srvHandle.Offset(1, m_cbvSrvDescriptorSize);

        m_device->CreateShaderResourceView(m_albedoRenderTarget.Get(), &srvDesc, srvHandle);
        srvHandle.Offset(1, m_cbvSrvDescriptorSize);
    }

#ifdef SSAO_ON
    m_pSSAO->Upload(m_device.Get(), m_commandList.Get(), m_cbvSrvUavHeap.Get(), m_pModel->cbvSrvOffsetEnd + 4, m_cbvSrvDescriptorSize);
    m_pIBL->Upload(m_device.Get(), m_commandList.Get(), m_cbvSrvUavHeap.Get(), m_pSSAO->ssaoCbvSrvOffsetEnd, m_cbvSrvDescriptorSize);
#else
    m_pIBL->Upload(m_device.Get(), ws_envMapFullPath, m_commandList.Get(), m_cbvSrvUavHeap.Get(), m_pModel->cbvSrvOffsetEnd + 4, m_cbvSrvDescriptorSize);
#endif // SSAO_ON
}

void ModelViewer::PreCompute()
{
    // PBR IBL Generate BRDFLUT.
    {
        ThrowIfFailed(m_commandAllocator->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

        m_commandList->SetGraphicsRootSignature(m_pIBL->GetComputeRootSignature());

        m_pIBL->GenerateBRDFLUT(m_device.Get(), GetAssetFullPath(L"BRDFLUT.cso"), m_commandList.Get(), m_cbvSrvUavHeap.Get());
    }

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists_1[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists_1), ppCommandLists_1);

    WaitForPreviousFrame();

    // Prefilter Environment Map.
    {
        ThrowIfFailed(m_commandAllocator->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

        m_commandList->SetGraphicsRootSignature(m_pIBL->GetComputeRootSignature());

        m_pIBL->PrefilterEnvMap(m_device.Get(), GetAssetFullPath(L"EnvironmentMap.cso"), m_commandList.Get(), m_cbvSrvUavHeap.Get(), m_cbvSrvDescriptorSize);
    }

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists_2[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists_2), ppCommandLists_2);

    WaitForPreviousFrame();

    // Irradiance Map.
    {
        ThrowIfFailed(m_commandAllocator->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

        m_commandList->SetGraphicsRootSignature(m_pIBL->GetComputeRootSignature());

        m_pIBL->IrradianceMap(m_device.Get(), GetAssetFullPath(L"IrradianceMap.cso"), m_commandList.Get(), m_cbvSrvUavHeap.Get());
    }

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists_3[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists_3), ppCommandLists_3);

    WaitForPreviousFrame();
}

void ModelViewer::OnUpdate()
{
    m_timer.Tick(NULL);

    if (m_frameCounter == 500)
    {
        // Update window text with FPS value.
        wchar_t fps[64];
        swprintf_s(fps, L"%ufps", m_timer.GetFramesPerSecond());
        SetCustomWindowText(fps);
        m_frameCounter = 0;
    }

    m_frameCounter++;

    m_pCamera->Update(static_cast<float>(m_timer.GetElapsedSeconds()), XM_PI / 3.0f, m_aspectRatio);
    m_pLight->Update();
}

void ModelViewer::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void ModelViewer::PopulateCommandList()
{
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvUavHeap.Get(), m_samplerHeap.Get() };

    // Skybox Pass.
    {
        ThrowIfFailed(m_commandAllocator->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_skyboxState.Get()));
        m_commandList->SetGraphicsRootSignature(m_skyboxRootSignature.Get());
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_albedoRenderTarget.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), FrameCount + 3, m_rtvDescriptorSize);

        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, TRUE, &dsvHandle);

        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        m_pCamera->Render(m_commandList.Get(), m_cbvSrvUavHeap.Get(), 0, m_cbvSrvDescriptorSize);
        m_commandList->SetGraphicsRootDescriptorTable(1, m_pIBL->envMapSrvGPUHandle);

        m_commandList->IASetVertexBuffers(0, 0, nullptr);
        m_commandList->IASetIndexBuffer(nullptr);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->DrawInstanced(36, 1, 0, 0);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_albedoRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    }

    // G-Buffer Pass
    {
        m_commandList->SetGraphicsRootSignature(m_gbufferRootSignature.Get());
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_positionDepthRenderTarget.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_normalRenderTarget.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_tangentRenderTarget.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_albedoRenderTarget.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle[4] =
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), FrameCount, m_rtvDescriptorSize),
            CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), FrameCount + 1, m_rtvDescriptorSize),
            CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), FrameCount + 2, m_rtvDescriptorSize),
            CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), FrameCount + 3, m_rtvDescriptorSize)
        };

        m_commandList->ClearRenderTargetView(rtvHandle[0], clearColor, 0, nullptr);
        m_commandList->ClearRenderTargetView(rtvHandle[1], clearColor, 0, nullptr);
        m_commandList->ClearRenderTargetView(rtvHandle[2], clearColor, 0, nullptr);
        //m_commandList->ClearRenderTargetView(rtvHandle[3], clearColor, 0, nullptr);
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        m_commandList->OMSetRenderTargets(4, &rtvHandle[0], FALSE, &dsvHandle);

        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        m_commandList->SetGraphicsRootDescriptorTable(9, m_pIBL->brdfLUTSrvGPUHandle);
        m_commandList->SetGraphicsRootDescriptorTable(10, m_pIBL->filteredEnvMapSrvGPUHandle);
        m_commandList->SetGraphicsRootDescriptorTable(11, m_pIBL->irMapSrvGPUHandle);

        m_commandList->SetPipelineState(m_gbufferState.Get());

        m_pCamera->Render(m_commandList.Get(), m_cbvSrvUavHeap.Get(), 0, m_cbvSrvDescriptorSize);
        m_pLight->Render(m_commandList.Get(), m_cbvSrvUavHeap.Get(), 1, m_cbvSrvDescriptorSize);
        m_pModel->Render(m_commandList.Get(), m_cbvSrvUavHeap.Get(), m_samplerHeap.Get(), 2, m_cbvSrvDescriptorSize);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_positionDepthRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_normalRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_tangentRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_albedoRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    }

#ifdef SSAO_ON
    // SSAO Pass.
    {
        m_commandList->SetGraphicsRootSignature(m_pSSAO->GetRootSignature());

        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), m_pModel->cbvSrvOffsetEnd, m_cbvSrvDescriptorSize);

        m_pCamera->Render(m_commandList.Get(), m_cbvSrvHeap.Get(), 0, m_cbvSrvDescriptorSize);
        m_pSSAO->Render(m_commandList.Get(), m_cbvSrvHeap.Get(), 1, m_cbvSrvDescriptorSize, srvHandle);
    }

    // SSAO Blur Pass.
    {
        m_commandList->SetGraphicsRootSignature(m_pSSAO->GetBlurRootSignature());

        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        m_pSSAO->Blur(m_commandList.Get(), m_cbvSrvHeap.Get(), 0, m_cbvSrvDescriptorSize);
    }

    // Final Pass.
    {
        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);

        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, TRUE, nullptr);

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), m_pModel->cbvSrvOffsetEnd + 3, m_cbvSrvDescriptorSize);
        m_commandList->SetGraphicsRootDescriptorTable(0, srvHandle);
        CD3DX12_GPU_DESCRIPTOR_HANDLE ssaoSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), m_pSSAO->ssaoCbvSrvOffset + 3, m_cbvSrvDescriptorSize);
        m_commandList->SetGraphicsRootDescriptorTable(1, ssaoSrvHandle);

        m_commandList->SetPipelineState(m_pipelineState.Get());

        m_commandList->IASetVertexBuffers(0, 0, nullptr);
        m_commandList->IASetIndexBuffer(nullptr);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->DrawInstanced(6, 1, 0, 0);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        ThrowIfFailed(m_commandList->Close());
    }
#else
    // Final Pass.
    {
        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);

        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, TRUE, nullptr);

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), m_pModel->cbvSrvOffsetEnd + 3, m_cbvSrvDescriptorSize);
        m_commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

        m_commandList->SetPipelineState(m_pipelineState.Get());

        m_commandList->IASetVertexBuffers(0, 0, nullptr);
        m_commandList->IASetIndexBuffer(nullptr);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->DrawInstanced(6, 1, 0, 0);

        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    }
#endif // SSAO_ON
}

void ModelViewer::OnDestroy()
{
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void ModelViewer::OnKeyDown(UINT8 key)
{
    m_pCamera->OnKeyDown(key);
}

void ModelViewer::OnKeyUp(UINT8 key)
{
    m_pCamera->OnKeyUp(key);
}

void ModelViewer::WaitForPreviousFrame()
{
    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
