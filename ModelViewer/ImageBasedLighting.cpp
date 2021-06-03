#include "stdafx.h"
#include "ImageBasedLighting.h"

DXImageBasedLighting::DXImageBasedLighting()
{
}

void DXImageBasedLighting::CreateRootSignature(ID3D12Device* device)
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // Create an empty root signature for generate BRDFLUT.
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&genBRDFLUTRootSignature)));
        NAME_D3D12_OBJECT(genBRDFLUTRootSignature);
    }
}

void DXImageBasedLighting::CreatePipelineState(ID3D12Device* device, 
    const std::wstring& vsGenBRDFLUTName, const std::wstring& psGenBRDFLUTName,
    const std::wstring& vsFilterCubeName, const std::wstring& psIrradianceCubeName, const std::wstring& psPreFilterEnvMapName,
    const std::wstring& vsPBRIBLName, const std::wstring& psPBRIBLName)
{
    // Generate BRDFLUT.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        ThrowIfFailed(D3DReadFileToBlob(vsGenBRDFLUTName.c_str(), &vertexShader));
        ThrowIfFailed(D3DReadFileToBlob(psGenBRDFLUTName.c_str(), &pixelShader));

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { nullptr, 0 };
        psoDesc.pRootSignature = genBRDFLUTRootSignature.Get();
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
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&genBRDFLUTState)));
        NAME_D3D12_OBJECT(genBRDFLUTState);
    }
}

INT DXImageBasedLighting::Init(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, UINT rtvDescriptorSize)
{
    INT rtvOffset = 0;

    genBRDFLUTRtvHandle = rtvHandle;
    D3D12_RESOURCE_DESC renderTargetDesc;
    renderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    renderTargetDesc.Alignment = 0;
    renderTargetDesc.Width = BRDFLUTDim;
    renderTargetDesc.Height = BRDFLUTDim;
    renderTargetDesc.DepthOrArraySize = 1;
    renderTargetDesc.MipLevels = 1;
    renderTargetDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
    renderTargetDesc.SampleDesc.Count = 1;
    renderTargetDesc.SampleDesc.Quality = 0;
    renderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    renderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    D3D12_CLEAR_VALUE clearValue = {};
    memcpy(clearValue.Color, clearColor, sizeof(clearColor));
    clearValue.Format = DXGI_FORMAT_R16G16_FLOAT;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &renderTargetDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&brdfLUTRenderTarget)
    ));
    NAME_D3D12_OBJECT(brdfLUTRenderTarget);
    device->CreateRenderTargetView(brdfLUTRenderTarget.Get(), &rtvDesc, genBRDFLUTRtvHandle);
    rtvHandle.Offset(1, rtvDescriptorSize);
    rtvOffset++;

    return rtvOffset;
}

void DXImageBasedLighting::PreCompute(ID3D12GraphicsCommandList* commandList)
{
    GenerateBRDFLUT(commandList);
}

void DXImageBasedLighting::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize)
{
    iblCbvSrvOffset = offsetInHeap;
    INT offset = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(
        cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), iblCbvSrvOffset, cbvSrvDescriptorSize);

    genBRDFLUTSrvHandle = cbvSrvHandle;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(brdfLUTRenderTarget.Get(), &srvDesc, genBRDFLUTSrvHandle);
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
    offset++;

    iblCbvSrvOffsetEnd = iblCbvSrvOffset + offset;
}

void DXImageBasedLighting::Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize, CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle)
{
}

void DXImageBasedLighting::GenerateBRDFLUT(ID3D12GraphicsCommandList* commandList)
{
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        brdfLUTRenderTarget.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(genBRDFLUTRtvHandle, clearColor, 0, nullptr);
    commandList->OMSetRenderTargets(1, &genBRDFLUTRtvHandle, FALSE, nullptr);

    commandList->SetPipelineState(genBRDFLUTState.Get());

    commandList->IASetVertexBuffers(0, 0, nullptr);
    commandList->IASetIndexBuffer(nullptr);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(6, 1, 0, 0);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        brdfLUTRenderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}
