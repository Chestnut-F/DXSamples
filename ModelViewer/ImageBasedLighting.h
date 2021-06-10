#pragma once
#include "stb_image.h"
#include "DXSampleHelper.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static const UINT IrradianceMapDim = 32u;
static const UINT BRDFLUTDim = 512u;
static const UINT FilteredEnvMapDim = 1024u;

struct SkyboxConstantBuffer
{
	XMFLOAT4X4 Model;
	float padding[48]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(SkyboxConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

class DXImageBasedLighting
{
public:
	DXImageBasedLighting();

	void CreateRootSignature(ID3D12Device* device);
	//void CreatePipelineState(ID3D12Device* device, const std::wstring& csBRDFLUTName);

	INT Init(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, UINT rtvDescriptorSize);
	void Update();
	void Upload(ID3D12Device* device, const std::wstring& fileFullPath,
		ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize);
	void Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap,
		INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize, CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle);

	void GenerateBRDFLUT(ID3D12Device* device, const std::wstring& csBRDFLUTName,
		ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvUavHeap);
	void PrefilterEnvMap(ID3D12Device* device, const std::wstring& csEnvMapame,
		ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvUavHeap, UINT cbvSrvDescriptorSize);
	void IrradianceMap(ID3D12Device* device, const std::wstring& csIrMapame,
		ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvUavHeap);

	INT iblCbvSrvUavOffset;
	INT iblCbvSrvUavOffsetEnd;

	//ID3D12RootSignature* GetRootSignature() { return iblRootSignature.Get(); }
	ID3D12RootSignature* GetComputeRootSignature() { return computeRootSignature.Get(); }

	CD3DX12_CPU_DESCRIPTOR_HANDLE brdfLUTSrvCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE brdfLUTSrvGPUHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE irMapSrvCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE irMapSrvGPUHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE filteredEnvMapSrvCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE filteredEnvMapSrvGPUHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE envMapSrvCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE envMapSrvGPUHandle;
private:
	ComPtr<ID3D12RootSignature> computeRootSignature;
	ComPtr<ID3D12RootSignature> rootSignature;

	ComPtr<ID3D12PipelineState> genBRDFLUTPipelineState;
	ComPtr<ID3D12PipelineState> prefilterEnvMapPipelineState;
	ComPtr<ID3D12PipelineState> irradianceMapPipelineState;
	ComPtr<ID3D12PipelineState> pipelineState;
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE brdfLUTUavCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE brdfLUTUavGPUHandle;
	ComPtr<ID3D12Resource> brdfLUT;

	CD3DX12_CPU_DESCRIPTOR_HANDLE filteredEnvMapUavCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE filteredEnvMapUavGPUHandle;
	ComPtr<ID3D12Resource> filteredEnvMap;

	std::unique_ptr<ScratchImage> envMap;
	std::vector<D3D12_SUBRESOURCE_DATA> envMapSubresources;
	ComPtr<ID3D12Resource> envMapTexture;
	ComPtr<ID3D12Resource> envMapUploadHeap;
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE irMapUavCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE irMapUavGPUHandle;
	ComPtr<ID3D12Resource> irMap;
};