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

    // Create a root signature for compute shader.
    {
        CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);

        CD3DX12_ROOT_PARAMETER1 rootParameters[3];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1]);
        rootParameters[2].InitAsConstants(2, 0);

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
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature)));
        NAME_D3D12_OBJECT(computeRootSignature);
    }
}

INT DXImageBasedLighting::Init(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, UINT rtvDescriptorSize)
{
    INT rtvOffset = 0;

    return rtvOffset;
}

void DXImageBasedLighting::Upload(ID3D12Device* device, const std::wstring& fileFullPath, 
    ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvUavHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize)
{
    iblCbvSrvUavOffset = offsetInHeap;
    INT offset = 0;
    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvUavCPUHandle(
        cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), offsetInHeap, cbvSrvDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavGPUHandle(
        cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), offsetInHeap, cbvSrvDescriptorSize);

    // Create BRDF Look-Up Table resource and view.
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = BRDFLUTDim;
        desc.Height = BRDFLUTDim;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R16G16_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&brdfLUT)
        ));
        NAME_D3D12_OBJECT(brdfLUT);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;

        device->CreateShaderResourceView(brdfLUT.Get(), &srvDesc, cbvSrvUavCPUHandle);
        brdfLUTSrvCPUHandle = cbvSrvUavCPUHandle;
        brdfLUTSrvGPUHandle = cbvSrvUavGPUHandle;
        cbvSrvUavCPUHandle.Offset(1, cbvSrvDescriptorSize);
        cbvSrvUavGPUHandle.Offset(1, cbvSrvDescriptorSize);
        offset++;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = desc.Format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;

        device->CreateUnorderedAccessView(brdfLUT.Get(), nullptr, &uavDesc, cbvSrvUavCPUHandle);
        brdfLUTUavCPUHandle = cbvSrvUavCPUHandle;
        brdfLUTUavGPUHandle = cbvSrvUavGPUHandle;
        cbvSrvUavCPUHandle.Offset(1, cbvSrvDescriptorSize);
        cbvSrvUavGPUHandle.Offset(1, cbvSrvDescriptorSize);
        offset++;
    }

    // Create Environment Map resource, upload heap and view.
    {
        TexMetadata info;
        envMap = std::make_unique<ScratchImage>();
        ThrowIfFailed(LoadFromDDSFile(fileFullPath.c_str(), DDS_FLAGS_NONE, &info, *envMap));

        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = static_cast<UINT16>(info.mipLevels);
        textureDesc.Format = info.format;
        textureDesc.Width = info.width;
        textureDesc.Height = static_cast<UINT>(info.height);
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = static_cast<UINT16>(info.arraySize);
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&envMapTexture)));
        NAME_D3D12_OBJECT(envMapTexture);

        ThrowIfFailed(PrepareUpload(device, envMap->GetImages(), envMap->GetImageCount(), info, envMapSubresources));
        UINT64 uploadBufferSize = GetRequiredIntermediateSize(envMapTexture.Get(), 0, static_cast<unsigned int>(envMapSubresources.size()));

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&envMapUploadHeap)));

        UpdateSubresources(
            commandList, 
            envMapTexture.Get(), 
            envMapUploadHeap.Get(),
            0, 
            0, 
            static_cast<unsigned int>(envMapSubresources.size()), 
            envMapSubresources.data());
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            envMapTexture.Get(), 
            D3D12_RESOURCE_STATE_COPY_DEST, 
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = textureDesc.MipLevels;

        device->CreateShaderResourceView(envMapTexture.Get(), &srvDesc, cbvSrvUavCPUHandle);
        envMapSrvCPUHandle = cbvSrvUavCPUHandle;
        envMapSrvGPUHandle = cbvSrvUavGPUHandle;
        cbvSrvUavCPUHandle.Offset(1, cbvSrvDescriptorSize);
        cbvSrvUavGPUHandle.Offset(1, cbvSrvDescriptorSize);
        offset++;
    }

    // Create Irradiance Map and view.
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = IrradianceMapDim;
        desc.Height = IrradianceMapDim;
        desc.DepthOrArraySize = 6;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            IID_PPV_ARGS(&irMap)
        ));
        NAME_D3D12_OBJECT(irMap);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = desc.MipLevels;
        srvDesc.TextureCube.MostDetailedMip = 0;
        device->CreateShaderResourceView(irMap.Get(), &srvDesc, cbvSrvUavCPUHandle);

        irMapSrvCPUHandle = cbvSrvUavCPUHandle;
        irMapSrvGPUHandle = cbvSrvUavGPUHandle;
        cbvSrvUavCPUHandle.Offset(1, cbvSrvDescriptorSize);
        cbvSrvUavGPUHandle.Offset(1, cbvSrvDescriptorSize);
        offset++;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = desc.Format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        uavDesc.Texture2DArray.MipSlice = 0;
        uavDesc.Texture2DArray.FirstArraySlice = 0;
        uavDesc.Texture2DArray.ArraySize = 6;
        device->CreateUnorderedAccessView(irMap.Get(), nullptr, &uavDesc, cbvSrvUavCPUHandle);

        irMapUavCPUHandle = cbvSrvUavCPUHandle;
        irMapUavGPUHandle = cbvSrvUavGPUHandle;
        cbvSrvUavCPUHandle.Offset(1, cbvSrvDescriptorSize);
        cbvSrvUavGPUHandle.Offset(1, cbvSrvDescriptorSize);
        offset++;
    }

    // Create Pre-filtered Environment Map resource and view.
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = FilteredEnvMapDim;
        desc.Height = FilteredEnvMapDim;
        desc.DepthOrArraySize = 6;
        desc.MipLevels = 11;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&filteredEnvMap)
        ));
        NAME_D3D12_OBJECT(filteredEnvMap);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = desc.MipLevels;
        srvDesc.TextureCube.MostDetailedMip = 0;
        device->CreateShaderResourceView(filteredEnvMap.Get(), &srvDesc, cbvSrvUavCPUHandle);

        filteredEnvMapSrvCPUHandle = cbvSrvUavCPUHandle;
        filteredEnvMapSrvGPUHandle = cbvSrvUavGPUHandle;
        cbvSrvUavCPUHandle.Offset(1, cbvSrvDescriptorSize);
        cbvSrvUavGPUHandle.Offset(1, cbvSrvDescriptorSize);
        offset++;

        filteredEnvMapUavCPUHandle = cbvSrvUavCPUHandle;
        filteredEnvMapUavGPUHandle = cbvSrvUavGPUHandle;
        offset++;
    }

    iblCbvSrvUavOffsetEnd = iblCbvSrvUavOffset + offset;
}

void DXImageBasedLighting::Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize, CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle)
{
}

void DXImageBasedLighting::GenerateBRDFLUT(ID3D12Device* device, const std::wstring& csBRDFLUTName, 
    ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvUavHeap)
{
    ComPtr<ID3DBlob> computeShader;

    ThrowIfFailed(D3DReadFileToBlob(csBRDFLUTName.c_str(), &computeShader));

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = computeRootSignature.Get();
    psoDesc.CS = CD3DX12_SHADER_BYTECODE{ computeShader.Get() };

    ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&genBRDFLUTPipelineState)));
    NAME_D3D12_OBJECT(genBRDFLUTPipelineState);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        brdfLUT.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    commandList->SetDescriptorHeaps(1, &cbvSrvUavHeap);
    commandList->SetPipelineState(genBRDFLUTPipelineState.Get());
    commandList->SetComputeRootSignature(computeRootSignature.Get());
    commandList->SetComputeRootDescriptorTable(1, brdfLUTUavGPUHandle);
    commandList->Dispatch(BRDFLUTDim / 32, BRDFLUTDim / 32, 1);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        brdfLUT.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));
}

void DXImageBasedLighting::PrefilterEnvMap(ID3D12Device* device, const std::wstring& csEnvMapame, 
    ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvUavHeap, UINT cbvSrvDescriptorSize)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavCPUHandle = filteredEnvMapUavCPUHandle;
    CD3DX12_GPU_DESCRIPTOR_HANDLE uavGPUHandle = filteredEnvMapUavGPUHandle;

    ComPtr<ID3DBlob> computeShader;

    ThrowIfFailed(D3DReadFileToBlob(csEnvMapame.c_str(), &computeShader));

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = computeRootSignature.Get();
    psoDesc.CS = CD3DX12_SHADER_BYTECODE{ computeShader.Get() };

    ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&prefilterEnvMapPipelineState)));
    NAME_D3D12_OBJECT(prefilterEnvMapPipelineState);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        filteredEnvMap.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        envMapTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE));

    for (UINT arraySlice = 0; arraySlice < 6; ++arraySlice) {
        const UINT subresourceIndex = D3D12CalcSubresource(0, arraySlice, 0, static_cast<UINT>(envMap->GetMetadata().mipLevels), 6u);
        commandList->CopyTextureRegion(
            &CD3DX12_TEXTURE_COPY_LOCATION{ filteredEnvMap.Get(), subresourceIndex },
            0, 
            0, 
            0, 
            &CD3DX12_TEXTURE_COPY_LOCATION{ envMapTexture.Get(), subresourceIndex },
            nullptr);
    }

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        envMapTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        filteredEnvMap.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    commandList->SetDescriptorHeaps(1, &cbvSrvUavHeap);
    commandList->SetPipelineState(prefilterEnvMapPipelineState.Get());
    commandList->SetComputeRootSignature(computeRootSignature.Get());
    commandList->SetComputeRootDescriptorTable(0, envMapSrvGPUHandle);

    const float deltaRoughness = 1.0f / (11.0f - 1.0f);
    for (UINT level = 1, size = 512; level < 11u; ++level, size /= 2) 
    {
        const UINT numGroups = std::max<UINT>(1, size / 32);
        const float spmapRoughness = level * deltaRoughness;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = envMap->GetMetadata().format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        uavDesc.Texture2DArray.MipSlice = level;
        uavDesc.Texture2DArray.FirstArraySlice = 0;
        uavDesc.Texture2DArray.ArraySize = 6;
        device->CreateUnorderedAccessView(filteredEnvMap.Get(), nullptr, &uavDesc, uavCPUHandle);

        commandList->SetComputeRootDescriptorTable(1, uavGPUHandle);
        commandList->SetComputeRoot32BitConstants(2, 1, &spmapRoughness, 0);
        commandList->Dispatch(numGroups, numGroups, 6);

        uavCPUHandle.Offset(1, cbvSrvDescriptorSize);
        uavGPUHandle.Offset(1, cbvSrvDescriptorSize);
        iblCbvSrvUavOffsetEnd++;
    }

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        filteredEnvMap.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));
}

void DXImageBasedLighting::IrradianceMap(ID3D12Device* device, const std::wstring& csIrMapame, 
    ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvUavHeap)
{
    ComPtr<ID3DBlob> computeShader;

    ThrowIfFailed(D3DReadFileToBlob(csIrMapame.c_str(), &computeShader));

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = computeRootSignature.Get();
    psoDesc.CS = CD3DX12_SHADER_BYTECODE{ computeShader.Get() };

    ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&irradianceMapPipelineState)));
    NAME_D3D12_OBJECT(irradianceMapPipelineState);

    commandList->SetDescriptorHeaps(1, &cbvSrvUavHeap);
    commandList->SetPipelineState(irradianceMapPipelineState.Get());
    commandList->SetComputeRootSignature(computeRootSignature.Get());
    commandList->SetComputeRootDescriptorTable(0, envMapSrvGPUHandle);
    commandList->SetComputeRootDescriptorTable(1, irMapUavGPUHandle);
    const float delta[2] = { (2.0f * XM_PI) / 180.0f, (0.5f * XM_PI) / 64.0f };
    commandList->SetComputeRoot32BitConstants(2, 2, &delta, 0);

    const UINT numGroups = std::max<UINT>(1, IrradianceMapDim / 32);
    commandList->Dispatch(numGroups, numGroups, 6);
    
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        irMap.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));
}
