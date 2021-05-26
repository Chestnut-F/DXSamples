#include "stdafx.h"
#include "SSAO.h"

SSAO::SSAO()
{
}

void SSAO::CreateRootSignature(ID3D12Device* device)
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // SSAO
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[6];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rootParameters[6];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[3].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[4].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[5].InitAsDescriptorTable(1, &ranges[5], D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
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
        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&ssaoRootSignature)));
        NAME_D3D12_OBJECT(ssaoRootSignature);
    }
    
    // SSAO Blur
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
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
        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&ssaoBlurRootSignature)));
        NAME_D3D12_OBJECT(ssaoBlurRootSignature);
    }
}

void SSAO::CreatePipelineState(ID3D12Device* device, const std::wstring& vsName, 
    const std::wstring& psName, const std::wstring& ps2Name)
{
    // SSAO
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        ThrowIfFailed(D3DReadFileToBlob(vsName.c_str(), &vertexShader));
        ThrowIfFailed(D3DReadFileToBlob(psName.c_str(), &pixelShader));

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { nullptr, 0 };
        psoDesc.pRootSignature = ssaoRootSignature.Get();
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

        ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ssaoState)));
        NAME_D3D12_OBJECT(ssaoState);
    }

    // SSAO Blur
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        ThrowIfFailed(D3DReadFileToBlob(vsName.c_str(), &vertexShader));
        ThrowIfFailed(D3DReadFileToBlob(ps2Name.c_str(), &pixelShader));

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { nullptr, 0 };
        psoDesc.pRootSignature = ssaoBlurRootSignature.Get();
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

        ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ssaoBlurState)));
        NAME_D3D12_OBJECT(ssaoBlurState);
    }
}

void SSAO::Init(ID3D12Device* device, UINT width, UINT height, ID3D12DescriptorHeap* rtvHeap, 
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, UINT rtvDescriptorSize)
{
    ssaoRtvHandle = rtvHandle;
    D3D12_RESOURCE_DESC renderTargetDesc;
    renderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    renderTargetDesc.Alignment = 0;
    renderTargetDesc.Width = width;
    renderTargetDesc.Height = height;
    renderTargetDesc.DepthOrArraySize = 1;
    renderTargetDesc.MipLevels = 1;
    renderTargetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    renderTargetDesc.SampleDesc.Count = 1;
    renderTargetDesc.SampleDesc.Quality = 0;
    renderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    renderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    D3D12_CLEAR_VALUE clearValue = {};
    memcpy(clearValue.Color, clearColor, sizeof(clearColor));
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &renderTargetDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&ssaoRenderTarget)
    ));
    NAME_D3D12_OBJECT(ssaoRenderTarget);
    device->CreateRenderTargetView(ssaoRenderTarget.Get(), &rtvDesc, ssaoRtvHandle);
    rtvHandle.Offset(1, rtvDescriptorSize);

    ssaoBlurRtvHandle = rtvHandle;
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &renderTargetDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&ssaoBlurRenderTarget)
    ));
    NAME_D3D12_OBJECT(ssaoBlurRenderTarget);
    device->CreateRenderTargetView(ssaoBlurRenderTarget.Get(), &rtvDesc, ssaoBlurRtvHandle);
}

void SSAO::Update()
{
}

void SSAO::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0, 1.0);
    for (uint32_t i = 0; i < SSAOKernelSize; ++i)
    {
        XMFLOAT3 sample(dis(gen) * 2.0f - 1.0f, dis(gen) * 2.0f - 1.0f, dis(gen));
        XMVECTOR sampleVector = XMVector3Normalize(XMLoadFloat3(&sample));
        sampleVector *= dis(gen);
        float scale = float(i) / float(SSAOKernelSize);
        scale = 0.1f + scale * scale * (1.0f - 0.1f);
        XMStoreFloat4(&ssaoKernel.ssaoKernel[i], sampleVector * scale);
    }

    ssaoCbvSrvOffset = offsetInHeap;
    const UINT constantBufferSize = sizeof(SSAOKernelConstantBuffer);

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ssaoConstantBuffer)));
    NAME_D3D12_OBJECT(ssaoConstantBuffer);

    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(
        cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), ssaoCbvSrvOffset, cbvSrvDescriptorSize);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = ssaoConstantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize;
    device->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(ssaoConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pSSAOCbvDataBegin)));
    memcpy(pSSAOCbvDataBegin, &ssaoKernel, sizeof(SSAOKernelConstantBuffer));
    ssaoConstantBuffer->Unmap(0, nullptr);

    // Random noise
    {
        for (uint32_t i = 0; i < SSAONoiseWidth * SSAONoiseHeight; i++)
        {
            ssaoNoise.ssaoNoise[i] = XMFLOAT4(
                dis(gen) * 2.0f - 1.0f, 
                dis(gen) * 2.0f - 1.0f, 
                dis(gen) * 2.0f - 1.0f, 
                0.0f);
        }

        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.Width = SSAONoiseWidth;
        textureDesc.Height = SSAONoiseHeight;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&ssaoNoiseTexture)));
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(ssaoNoiseTexture.Get(), 0, 1);

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&ssaoNoiseTextureUploadHeap)));

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &ssaoNoise.ssaoNoise[0];
        textureData.RowPitch = SSAONoiseWidth * sizeof(XMFLOAT4);
        textureData.SlicePitch = textureData.RowPitch * SSAONoiseHeight;

        UpdateSubresources(commandList, ssaoNoiseTexture.Get(), ssaoNoiseTextureUploadHeap.Get(), 0, 0, 1, &textureData);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            ssaoNoiseTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(ssaoNoiseTexture.Get(), &srvDesc, cbvSrvHandle);
        cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
    }

    ssaoSrvHandle = cbvSrvHandle;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(ssaoRenderTarget.Get(), &srvDesc, ssaoSrvHandle);
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);

    ssaoBlurSrvHandle = cbvSrvHandle;
    device->CreateShaderResourceView(ssaoBlurRenderTarget.Get(), &srvDesc, ssaoBlurSrvHandle);
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);

    ssaoCbvSrvOffsetEnd = ssaoCbvSrvOffset + 4;
}

void SSAO::Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize, CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle)
{
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ssaoRenderTarget.Get(), 
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(ssaoRtvHandle, clearColor, 0, nullptr);
    commandList->OMSetRenderTargets(1, &ssaoRtvHandle, TRUE, nullptr);

    CD3DX12_GPU_DESCRIPTOR_HANDLE ssaoCbvSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), ssaoCbvSrvOffset, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable, ssaoCbvSrvHandle);
    ssaoCbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable + 1, srvHandle);
    srvHandle.Offset(1, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable + 2, srvHandle);
    srvHandle.Offset(1, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable + 3, srvHandle);
    commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable + 4, ssaoCbvSrvHandle);

    commandList->SetPipelineState(ssaoState.Get());

    commandList->IASetVertexBuffers(0, 0, nullptr);
    commandList->IASetIndexBuffer(nullptr);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(6, 1, 0, 0);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ssaoRenderTarget.Get(), 
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}

void SSAO::Blur(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize)
{
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ssaoBlurRenderTarget.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(ssaoBlurRtvHandle, clearColor, 0, nullptr);
    commandList->OMSetRenderTargets(1, &ssaoBlurRtvHandle, TRUE, nullptr);

    CD3DX12_GPU_DESCRIPTOR_HANDLE ssaoCbvSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), ssaoCbvSrvOffset + 2, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(0, ssaoCbvSrvHandle);

    commandList->SetPipelineState(ssaoBlurState.Get());

    commandList->IASetVertexBuffers(0, 0, nullptr);
    commandList->IASetIndexBuffer(nullptr);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(6, 1, 0, 0);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        ssaoBlurRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}
