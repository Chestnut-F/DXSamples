#pragma once
#include "tiny_gltf.h"
#include "GltfHelper.h"
#include "DXSampleHelper.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct MaterialConstantBuffer
{
	XMFLOAT4 BaseColorFactor;
	float padding[60]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(MaterialConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct GlobalConstantBuffer
{
	XMFLOAT4X4 ModelViewProj;
	XMFLOAT4X4 Model;
	XMFLOAT3 EyePosW;
	float padding[29]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(GlobalConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

class DXMaterial
{
public:
	DXMaterial(const tinygltf::Model& model, const tinygltf::Material material);
	~DXMaterial();
	void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
		ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, INT offetInPrimitives, UINT cbvSrvDescriptorSize);
	void Draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
		INT offsetInDescriptors, UINT cbvSrvDescriptorSize);
private:
	const tinygltf::Model* pModel;

	const tinygltf::Image* baseColorImage;
	//const tinygltf::Image* metallicRoughnessImage;
	//const tinygltf::Image* emissiveImage;
	//const tinygltf::Image* normalImage;
	//const tinygltf::Image* occlusionImage;

	DirectX::XMFLOAT4 baseColorFactor;
	//float metallicFactor;
	//float roughnessFactor;
	//DirectX::XMFLOAT3 emissiveFactor;

	//float normalScale;
	//float occlusionStrength;

	UINT materialCbvOffset;
	ComPtr<ID3D12Resource> constantBuffer;
	UINT8* pMaterialCbvDataBegin;

	UINT baseColorTextureViewOffset;
	//UINT metallicRoughnessViewOffset;
	//UINT emissiveTextureViewOffset;
	//UINT normalTextureViewOffset;
	//UINT occlusionTextureViewOffset;

	ComPtr<ID3D12Resource> baseColorTexture;
	//ComPtr<ID3D12Resource> metallicRoughnessTexture;
	//ComPtr<ID3D12Resource> emissiveTexture;
	//ComPtr<ID3D12Resource> normalTexture;
	//ComPtr<ID3D12Resource> occlusionTexture;

	ComPtr<ID3D12Resource> baseColorTextureUploadHeap;
	//ComPtr<ID3D12Resource> metallicRoughnessTextureUploadHeap;
	//ComPtr<ID3D12Resource> emissiveTextureUploadHeap;
	//ComPtr<ID3D12Resource> normalTextureUploadHeap;
	//ComPtr<ID3D12Resource> occlusionTextureUploadHeap;
};

class DXPrimitive
{
public:
	DXPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, XMMATRIX localTransform);
	~DXPrimitive();
	void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
		ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, INT offetInPrimitives, UINT cbvSrvDescriptorSize);
	void Update(XMFLOAT3 eyePosW, XMMATRIX view, XMMATRIX proj);
	void Draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
		ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize);
private:
	const tinygltf::Model* pModel;
	std::vector<GltfHelper::Vertex> vertices;
	std::vector<UINT> indices;
	std::shared_ptr<DXMaterial> pMaterial;

	UINT globalCbvOffset;
	XMFLOAT4X4 localTransform;
	GlobalConstantBuffer globalConstantBuffer;
	UINT8* pGlobalCbvDataBegin;

	UINT numIndices;
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> indexBuffer;
	ComPtr<ID3D12Resource> vertexBufferUploadHeap;
	ComPtr<ID3D12Resource> indexBufferUploadHeap;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	ComPtr<ID3D12Resource> constantBuffer;
};

class DXMesh
{
public:
	DXMesh(const tinygltf::Model& model, const tinygltf::Node& node, const tinygltf::Mesh& mesh);
	void Update(XMFLOAT3 eyePosW, XMMATRIX view, XMMATRIX proj);
	void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
		ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, INT offetInPrimitives, UINT cbvSrvDescriptorSize);
	void Draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
		ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize);

	std::vector<DXPrimitive> primitives;
	UINT primitiveSize;
private:
	const tinygltf::Model* pModel;
};

class DXModel
{
public:
	DXModel(const std::string& assetFullPath);
	~DXModel();
	void Update(XMFLOAT3 eyePosW, XMMATRIX view, XMMATRIX proj);
	void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, 
		ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize);
	void Draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, 
		ID3D12DescriptorHeap* samplerHeap, INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize);

	std::vector<DXMesh> meshes;
	UINT meshSize;
	UINT primitiveSize;
private:
	tinygltf::Model* pModel;

	void ProcessModel();
	void ProcessNode(const tinygltf::Node& node);
	void ProcessMesh(const tinygltf::Node& node, const tinygltf::Mesh& mesh);
};