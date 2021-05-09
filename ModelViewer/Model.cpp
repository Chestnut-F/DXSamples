#include "stdafx.h"
#include "Model.h"

DXMaterial::DXMaterial(const tinygltf::Model& model, const tinygltf::Material material)
{
    this->pModel = &model;
    GltfHelper::Material gltfHelperMaterial = GltfHelper::ReadMaterial(model, material);
    baseColorFactor = gltfHelperMaterial.BaseColorFactor;
    if (gltfHelperMaterial.BaseColorTexture.Image)
    {
        baseColorImage = gltfHelperMaterial.BaseColorTexture.Image;
    }
}

DXMaterial::~DXMaterial()
{
}

void DXMaterial::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, INT offetInPrimitives, UINT cbvSrvDescriptorSize)
{
    materialCbvOffset = offsetInHeap + offetInPrimitives * 3 + 1;
    MaterialConstantBuffer materialConstantBuffer;
    materialConstantBuffer.BaseColorFactor = baseColorFactor;
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

        CD3DX12_CPU_DESCRIPTOR_HANDLE globalCbvHandle(cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), materialCbvOffset, cbvSrvDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = sizeof(MaterialConstantBuffer);
        device->CreateConstantBufferView(&cbvDesc, globalCbvHandle);

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pMaterialCbvDataBegin)));
        memcpy(pMaterialCbvDataBegin, &materialConstantBuffer, sizeof(MaterialConstantBuffer));
        constantBuffer->Unmap(0, 0);
    }

    baseColorTextureViewOffset = offsetInHeap + offetInPrimitives * 3 + 2;
    //Upload base color texture.
    if (baseColorImage != nullptr)
    {
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = static_cast<UINT>(baseColorImage->width);
        textureDesc.Height = static_cast<UINT>(baseColorImage->height);
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
            IID_PPV_ARGS(&baseColorTexture)));

        NAME_D3D12_OBJECT(baseColorTexture);

        UINT64 uploadBufferSize = GetRequiredIntermediateSize(baseColorTexture.Get(), 0, 1);

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&baseColorTextureUploadHeap)));

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = baseColorImage->image.data();
        textureData.RowPitch = static_cast<UINT>(baseColorImage->width) * (UINT)4;
        textureData.SlicePitch = static_cast<UINT>(baseColorImage->width) * static_cast<UINT>(baseColorImage->height) * (UINT)4;

        UpdateSubresources<1>(commandList, baseColorTexture.Get(), baseColorTextureUploadHeap.Get(), 0, 0, 1, &textureData);
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(baseColorTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), baseColorTextureViewOffset, cbvSrvDescriptorSize);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(baseColorTexture.Get(), &srvDesc, srvHandle);
    }
}

void DXMaterial::Draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInDescriptors, UINT cbvSrvDescriptorSize)
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvHandle(cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), materialCbvOffset, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInDescriptors + 0, cbvSrvHandle);
    cbvSrvHandle.Offset(1, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInDescriptors + 1, cbvSrvHandle);
}

DXPrimitive::DXPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, XMMATRIX localTrans)
{
    this->pModel = &model;
    XMStoreFloat4x4(&this->localTransform, localTrans);

    GltfHelper::Primitive helperPrimitive = GltfHelper::ReadPrimitive(model, primitive);

    std::copy(helperPrimitive.Vertices.begin(), helperPrimitive.Vertices.end(), std::back_inserter(this->vertices));
    std::copy(helperPrimitive.Indices.begin(), helperPrimitive.Indices.end(), std::back_inserter(this->indices));

    pMaterial = std::make_shared<DXMaterial>(model, model.materials[primitive.material]);
}

DXPrimitive::~DXPrimitive()
{
}

void DXPrimitive::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, INT offetInPrimitives, UINT cbvSrvDescriptorSize)
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
    
    globalCbvOffset = offsetInHeap + offetInPrimitives * 3 + 0;
    {
        const UINT constantBufferSize = sizeof(GlobalConstantBuffer);    // CB size is required to be 256-byte aligned.

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&constantBuffer)));

        NAME_D3D12_OBJECT(constantBuffer);

        CD3DX12_CPU_DESCRIPTOR_HANDLE globalCbvHandle(cbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), globalCbvOffset, cbvSrvDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = sizeof(GlobalConstantBuffer);
        device->CreateConstantBufferView(&cbvDesc, globalCbvHandle);

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pGlobalCbvDataBegin)));
    }

    pMaterial->Upload(device, commandList, cbvSrvHeap, offsetInHeap, offetInPrimitives, cbvSrvDescriptorSize);
}

void DXPrimitive::Update(XMFLOAT3 eyePosW, XMMATRIX view, XMMATRIX proj)
{
    XMMATRIX mod = XMLoadFloat4x4(&localTransform);
    globalConstantBuffer.EyePosW = eyePosW;
    XMStoreFloat4x4(&globalConstantBuffer.ModelViewProj, XMMatrixTranspose(mod * view * proj));
    memcpy(pGlobalCbvDataBegin, &globalConstantBuffer, sizeof(GlobalConstantBuffer));
}

void DXPrimitive::Draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize)
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvHandle(cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), globalCbvOffset, cbvSrvDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(offsetInRootDescriptorTable, cbvSrvHandle);

    pMaterial->Draw(commandList, cbvSrvHeap, offsetInRootDescriptorTable + 1, cbvSrvDescriptorSize);

    commandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}

DXMesh::DXMesh(const tinygltf::Model& model, const tinygltf::Node& node, const tinygltf::Mesh& mesh)
{
    this->pModel = &model;
    XMMATRIX localTrans = GltfHelper::ReadNodeLocalTransform(node);

    for (UINT i = 0; i < mesh.primitives.size(); ++i)
    {
        const tinygltf::Primitive& tinyPrimitive = mesh.primitives[i];
        primitives.emplace_back(model, tinyPrimitive, localTrans);
    }

    primitiveSize = static_cast<UINT>(primitives.size());
}

void DXMesh::Update(XMFLOAT3 eyePosW, XMMATRIX view, XMMATRIX proj)
{
    for (UINT i = 0; i < primitiveSize; ++i)
    {
        primitives[i].Update(eyePosW, view, proj);
    }
}

void DXMesh::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, INT offetInPrimitives, UINT cbvSrvDescriptorSize)
{
    for (UINT i = 0; i < primitiveSize; ++i)
    {
        primitives[i].Upload(device, commandList, cbvSrvHeap, offsetInHeap, offetInPrimitives + i, cbvSrvDescriptorSize);
    }
}

void DXMesh::Draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize)
{
    for (UINT i = 0; i < primitiveSize; ++i)
    {
        primitives[i].Draw(commandList, cbvSrvHeap, samplerHeap, offsetInRootDescriptorTable, cbvSrvDescriptorSize);
    }
}

DXModel::DXModel(const std::string& assetFullPath):
    primitiveSize(0)
{
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    pModel = new tinygltf::Model();

    bool res = loader.LoadASCIIFromFile(pModel, &err, &warn, assetFullPath.c_str());
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

    meshSize = static_cast<UINT>(meshes.size());
}

DXModel::~DXModel()
{
    delete pModel;
}

void DXModel::ProcessModel()
{
    const tinygltf::Scene& scene = pModel->scenes[pModel->defaultScene];
    for (UINT i = 0; i < scene.nodes.size(); ++i) {
        ProcessNode(pModel->nodes[scene.nodes[i]]);
    }
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
    DXMesh dxMesh(*pModel, node, mesh);
    meshes.emplace_back(*pModel, node, mesh);
    primitiveSize += dxMesh.primitiveSize;
}

void DXModel::Update(XMFLOAT3 eyePosW, XMMATRIX view, XMMATRIX proj)
{
    for (UINT i = 0; i < meshSize; ++i)
    {
        meshes[i].Update(eyePosW, view, proj);
    }
}

void DXModel::Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
    ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize)
{
    UINT offetInPrimitives = 0;
    for (UINT i = 0; i < meshSize; ++i)
    {
        meshes[i].Upload(device, commandList, cbvSrvHeap, offsetInHeap, offetInPrimitives, cbvSrvDescriptorSize);
        offetInPrimitives += meshes[i].primitiveSize;
    }
}

void DXModel::Draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
    ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize)
{
    for (UINT i = 0; i < meshSize; ++i)
    {
        meshes[i].Draw(commandList, cbvSrvHeap, samplerHeap, offsetInRootDescriptorTable, cbvSrvDescriptorSize);
    }
}
