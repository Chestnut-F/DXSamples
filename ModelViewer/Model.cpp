#include "stdafx.h"
#include "Model.h"

DXMaterial::DXMaterial(std::shared_ptr<tinygltf::Model> model, const tinygltf::Material& material):
    baseColorImage(nullptr),
    metallicRoughnessImage(nullptr),
    emissiveImage(nullptr),
    normalImage(nullptr),
    occlusionImage(nullptr)
{
    this->pModel = model;
    GltfHelper::Material gltfHelperMaterial = GltfHelper::ReadMaterial(*model, material);

    materialConstant.BaseColorFactor = gltfHelperMaterial.BaseColorFactor;
    materialConstant.EmissiveFactor = gltfHelperMaterial.EmissiveFactor;
    materialConstant.MetallicFactor = gltfHelperMaterial.MetallicFactor;
    materialConstant.NormalScale = gltfHelperMaterial.NormalScale;
    materialConstant.OcclusionStrength = gltfHelperMaterial.OcclusionStrength;
    materialConstant.RoughnessFactor = gltfHelperMaterial.RoughnessFactor;

    if (gltfHelperMaterial.BaseColorTexture.Image != nullptr)
        baseColorImage = gltfHelperMaterial.BaseColorTexture.Image;
    if (gltfHelperMaterial.MetallicRoughnessTexture.Image != nullptr)
        metallicRoughnessImage = gltfHelperMaterial.MetallicRoughnessTexture.Image;
    if (gltfHelperMaterial.EmissiveTexture.Image != nullptr)
        emissiveImage = gltfHelperMaterial.EmissiveTexture.Image;
    if (gltfHelperMaterial.NormalTexture.Image != nullptr)
        normalImage = gltfHelperMaterial.NormalTexture.Image;
    if (gltfHelperMaterial.OcclusionTexture.Image != nullptr)
        occlusionImage = gltfHelperMaterial.OcclusionTexture.Image;
}

DXMaterial::~DXMaterial()
{
}

void DXMaterial::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize)
{
    materialCbvOffset = offsetInHeap;
    {
        const UINT constantBufferSize = sizeof(MaterialConstantBuffer);    // CB size is required to be 256-byte aligned.

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&constantBuffer)));

        NAME_D3D12_OBJECT(constantBuffer);

        CD3DX12_CPU_DESCRIPTOR_HANDLE materialCbvHandle(cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), materialCbvOffset, cbvSrvDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = sizeof(MaterialConstantBuffer);
        device->CreateConstantBufferView(&cbvDesc, materialCbvHandle);

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pMaterialCbvDataBegin)));
        memcpy(pMaterialCbvDataBegin, &materialConstant, constantBufferSize);
        constantBuffer->Unmap(0, 0);
    }

    if (baseColorImage != nullptr)
    {
        UploadImage(device, commandList, cbvSrvHeap, 1, cbvSrvDescriptorSize, 
            baseColorImage, baseColorTexture.Get(), baseColorTextureUploadHeap.Get());
    }
    if (metallicRoughnessImage != nullptr)
    {
        UploadImage(device, commandList, cbvSrvHeap, 2, cbvSrvDescriptorSize,
            metallicRoughnessImage, metallicRoughnessTexture.Get(), metallicRoughnessTextureUploadHeap.Get());
    }
    if (emissiveImage != nullptr)
    {
        UploadImage(device, commandList, cbvSrvHeap, 3, cbvSrvDescriptorSize,
            emissiveImage, emissiveTexture.Get(), emissiveTextureUploadHeap.Get());
    }
    if (normalImage != nullptr)
    {
        UploadImage(device, commandList, cbvSrvHeap, 4, cbvSrvDescriptorSize,
            normalImage, normalTexture.Get(), normalTextureUploadHeap.Get());
    }
    if (occlusionImage != nullptr)
    {
        UploadImage(device, commandList, cbvSrvHeap, 5, cbvSrvDescriptorSize,
            occlusionImage, occlusionTexture.Get(), occlusionTextureUploadHeap.Get());
    }
}

void DXMaterial::UploadImage(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInMaterial, UINT cbvSrvDescriptorSize,
    const tinygltf::Image* image, ID3D12Resource* texture, ID3D12Resource* uploadHeap)
{
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = static_cast<UINT>(image->width);
    textureDesc.Height = static_cast<UINT>(image->height);
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
        IID_PPV_ARGS(&texture)));

    UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture, 0, 1);

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadHeap)));

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = image->image.data();
    textureData.RowPitch = static_cast<UINT>(image->width) * (UINT)4;
    textureData.SlicePitch = static_cast<UINT>(image->width) * static_cast<UINT>(image->height) * (UINT)4;

    UpdateSubresources<1>(commandList, texture, uploadHeap, 0, 0, 1, &textureData);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), materialCbvOffset + offsetInMaterial, cbvSrvDescriptorSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(texture, &srvDesc, srvHandle);
}

void DXMaterial::Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvHandle(cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), materialCbvOffset, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable, cbvSrvHandle);
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
    if (baseColorImage != nullptr)
    {
        commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable + 1, cbvSrvHandle);
    }
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
    if (metallicRoughnessImage != nullptr)
    {
        commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable + 2, cbvSrvHandle);
    }
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
    if (emissiveImage != nullptr)
    {
        commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable + 3, cbvSrvHandle);
    }
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
    if (normalImage != nullptr)
    {
        commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable + 4, cbvSrvHandle);
    }
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
    if (occlusionImage != nullptr)
    {
        commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable + 5, cbvSrvHandle);
    }
}

DXPrimitive::DXPrimitive(std::shared_ptr<tinygltf::Model> model, 
    const tinygltf::Primitive& primitive, XMMATRIX localTrans)
{
    this->pModel = model;
    XMStoreFloat4x4(&primitiveConstant.Model, localTrans);

    GltfHelper::Primitive helperPrimitive = GltfHelper::ReadPrimitive(*model, primitive);

    std::copy(helperPrimitive.Vertices.begin(), helperPrimitive.Vertices.end(), std::back_inserter(this->vertices));
    std::copy(helperPrimitive.Indices.begin(), helperPrimitive.Indices.end(), std::back_inserter(this->indices));

    indexMaterial = static_cast<UINT>(primitive.material);
}

DXPrimitive::~DXPrimitive()
{
}

void DXPrimitive::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize)
{
    {
        UINT vertexDataSize = static_cast<UINT>(vertices.size() * sizeof(GltfHelper::Vertex));

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexDataSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer)));

        NAME_D3D12_OBJECT(vertexBuffer);

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexDataSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBufferUploadHeap)));

        D3D12_SUBRESOURCE_DATA vertexData = {};
        vertexData.pData = vertices.data();
        vertexData.RowPitch = vertexDataSize;
        vertexData.SlicePitch = vertexData.RowPitch;

        UpdateSubresources<1>(commandList, vertexBuffer.Get(), vertexBufferUploadHeap.Get(), 0, 0, 1, &vertexData);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

        vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        vertexBufferView.StrideInBytes = static_cast<UINT>(sizeof(GltfHelper::Vertex));
        vertexBufferView.SizeInBytes = vertexDataSize;
    }

    {
        UINT indexDataSize = static_cast<UINT>(indices.size() * sizeof(UINT32));

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(indexDataSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&indexBuffer)));

        NAME_D3D12_OBJECT(indexBuffer);

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(indexDataSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&indexBufferUploadHeap)));

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the index buffer.
        D3D12_SUBRESOURCE_DATA indexData = {};
        indexData.pData = indices.data();
        indexData.RowPitch = indexDataSize;
        indexData.SlicePitch = indexDataSize;

        UpdateSubresources<1>(commandList, indexBuffer.Get(), indexBufferUploadHeap.Get(), 0, 0, 1, &indexData);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

        // Describe the index buffer view.
        indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
        indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        indexBufferView.SizeInBytes = indexDataSize;

        numIndices = indexDataSize / 4;    // R32_UINT (SampleAssets::StandardIndexFormat) = 4 bytes each.
    }
    
    primitiveCbvOffset = offsetInHeap;
    {
        const UINT constantBufferSize = sizeof(PrimitiveConstantBuffer);    // CB size is required to be 256-byte aligned.

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&constantBuffer)));

        NAME_D3D12_OBJECT(constantBuffer);

        CD3DX12_CPU_DESCRIPTOR_HANDLE primitiveCbvHandle(cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), primitiveCbvOffset, cbvSrvDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantBufferSize;
        device->CreateConstantBufferView(&cbvDesc, primitiveCbvHandle);

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pPrimitiveCbvDataBegin)));
        memcpy(pPrimitiveCbvDataBegin, &primitiveConstant, constantBufferSize);
        constantBuffer->Unmap(0, 0);
    }
}

void DXPrimitive::Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, ID3D12DescriptorHeap *samplerHeap, 
    INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize, DXMaterial* material)
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvHandle(cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), primitiveCbvOffset, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable, cbvSrvHandle);

    material->Render(commandList, cbvSrvHeap, samplerHeap, offsetInRootDescriptorTable + 1, cbvSrvDescriptorSize);

    commandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}

DXModel::DXModel(const std::string& assetFullPath):
    primitiveSize(0)
{
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    pModel = std::make_shared<tinygltf::Model>();

    bool res = loader.LoadASCIIFromFile(pModel.get(), &err, &warn, assetFullPath.c_str());
    if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cout << "ERR: " << err << std::endl;
    }

    if (!res)
        std::cout << "Failed to load glTF: " << "BoxTextured.gltf" << std::endl;
    else
        std::cout << "Loaded glTF: " << "BoxTextured.gltf" << std::endl;

    ProcessModel();
}

DXModel::~DXModel()
{
}

void DXModel::ProcessModel()
{
    const tinygltf::Scene& scene = pModel->scenes[pModel->defaultScene];
    for (UINT i = 0; i < scene.nodes.size(); ++i) {
        ProcessNode(pModel->nodes[scene.nodes[i]]);
    }
    ProcessMaterial();
}

void DXModel::ProcessNode(const tinygltf::Node& node)
{
    if ((node.mesh >= 0) && (node.mesh < pModel->meshes.size())) {
        ProcessMesh(node, pModel->meshes[node.mesh]);
    }
    for (UINT i = 0; i < node.children.size(); i++) {
        ProcessNode(pModel->nodes[node.children[i]]);
    }
}

void DXModel::ProcessMesh(const tinygltf::Node& node, const tinygltf::Mesh& mesh)
{
    XMMATRIX localTrans = GltfHelper::ReadNodeLocalTransform(node);

    for (UINT i = 0; i < mesh.primitives.size(); ++i)
    {
        const tinygltf::Primitive& tinyPrimitive = mesh.primitives[i];
        primitives.emplace_back(pModel, tinyPrimitive, localTrans);
    }

    primitiveSize += static_cast<UINT>(primitives.size());
}

void DXModel::ProcessMaterial()
{
    materialSize = static_cast<UINT>(pModel->materials.size());

    for (UINT i = 0; i < materialSize; ++i)
    {
        materials.emplace_back(pModel, pModel->materials[i]);
    }
}

void DXModel::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize)
{
    UINT offetInPrimitives = offsetInHeap;
    for (UINT i = 0; i < primitiveSize; ++i)
    {
        primitives[i].Upload(device, commandList, cbvSrvHeap, offetInPrimitives, cbvSrvDescriptorSize);
        ++offetInPrimitives;
    }

    UINT offsetInMaterials = offetInPrimitives;
    for (UINT i = 0; i < materials.size(); ++i)
    {
        materials[i].Upload(device, commandList, cbvSrvHeap, offsetInMaterials, cbvSrvDescriptorSize);
        offsetInMaterials += 6;
    }
}

void DXModel::Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize)
{
    for (UINT i = 0; i < primitiveSize; ++i)
    {
        UINT index = primitives[i].indexMaterial;
        primitives[i].Render(commandList, cbvSrvHeap, samplerHeap, 
            offsetInRootDescriptorTable, cbvSrvDescriptorSize, &materials[index]);
    }
}
