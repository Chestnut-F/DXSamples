#pragma once
#include "tiny_gltf.h"
#include "GltfHelper.h"
#include "DXSampleHelper.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct MaterialConstantBuffer
{
	XMFLOAT4 BaseColorFactor;
	float MetallicFactor;
	float RoughnessFactor;
	float NormalScale;
	float OcclusionStrength;
	XMFLOAT3 EmissiveFactor;
	float padding[53]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(MaterialConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct PrimitiveConstantBuffer
{
	XMFLOAT4X4 Model;
	float padding[48]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(PrimitiveConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

class DXMaterial
{
public:
	DXMaterial(std::shared_ptr<tinygltf::Model> model, const tinygltf::Material& material);
	~DXMaterial();
	void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
		ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize);
	void Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
		ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize);
private:
	std::shared_ptr<tinygltf::Model> pModel;

	const tinygltf::Image* baseColorImage;
	const tinygltf::Image* metallicRoughnessImage;
	const tinygltf::Image* emissiveImage;
	const tinygltf::Image* normalImage;
	const tinygltf::Image* occlusionImage;
	MaterialConstantBuffer materialConstant;

	UINT materialCbvOffset;
	ComPtr<ID3D12Resource> constantBuffer;
	UINT8* pMaterialCbvDataBegin;

	ComPtr<ID3D12Resource> baseColorTexture;
	ComPtr<ID3D12Resource> metallicRoughnessTexture;
	ComPtr<ID3D12Resource> emissiveTexture;
	ComPtr<ID3D12Resource> normalTexture;
	ComPtr<ID3D12Resource> occlusionTexture;

	ComPtr<ID3D12Resource> baseColorTextureUploadHeap;
	ComPtr<ID3D12Resource> metallicRoughnessTextureUploadHeap;
	ComPtr<ID3D12Resource> emissiveTextureUploadHeap;
	ComPtr<ID3D12Resource> normalTextureUploadHeap;
	ComPtr<ID3D12Resource> occlusionTextureUploadHeap;
};

class DXPrimitive
{
public:
	DXPrimitive(std::shared_ptr<tinygltf::Model> model, 
		const tinygltf::Primitive& primitive, XMMATRIX localTransform);
	~DXPrimitive();
	void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
		ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize);
	void Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, ID3D12DescriptorHeap* samplerHeap,
		INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize, DXMaterial* material);

	UINT indexMaterial;
private:
	std::shared_ptr<tinygltf::Model> pModel;
	std::vector<GltfHelper::Vertex> vertices;
	std::vector<UINT> indices;
	PrimitiveConstantBuffer primitiveConstant;

	UINT primitiveCbvOffset;
	UINT8* pPrimitiveCbvDataBegin;
	
	UINT numIndices;
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> indexBuffer;
	ComPtr<ID3D12Resource> vertexBufferUploadHeap;
	ComPtr<ID3D12Resource> indexBufferUploadHeap;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	ComPtr<ID3D12Resource> constantBuffer;
};

class DXModel
{
public:
	DXModel(const std::string& assetFullPath);
	~DXModel();
	void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
		ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize);
	void Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap,
		ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize);

	UINT meshSize;
	UINT primitiveSize;
	UINT materialSize;
private:
	std::shared_ptr<tinygltf::Model> pModel;
	std::vector<DXPrimitive> primitives;
	std::vector<DXMaterial> materials;

	void ProcessModel();
	void ProcessNode(const tinygltf::Node& node);
	void ProcessMesh(const tinygltf::Node& node, const tinygltf::Mesh& mesh);
	void ProcessMaterial();
};