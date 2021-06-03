#pragma once
#include "DXSampleHelper.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static const UINT BRDFLUTDim = 512;

class DXImageBasedLighting
{
public:
	DXImageBasedLighting();

	void CreateRootSignature(ID3D12Device* device);
	void CreatePipelineState(ID3D12Device* device, const std::wstring& vsGenBRDFLUTName, const std::wstring& psGenBRDFLUTName,
		const std::wstring& vsFilterCubeName, const std::wstring& psIrradianceCubeName, const std::wstring& psPreFilterEnvMapName,
		const std::wstring& vsPBRIBLName, const std::wstring& psPBRIBLName);

	INT Init(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, UINT rtvDescriptorSize);
	void PreCompute(ID3D12GraphicsCommandList* commandList);
	void Update();
	void Upload(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
		ID3D12DescriptorHeap* cbvSrvHeap, INT offsetInHeap, UINT cbvSrvDescriptorSize);
	void Render(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* cbvSrvHeap,
		INT offsetInRootDescriptorTable, UINT cbvSrvDescriptorSize, CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle);

	INT iblCbvSrvOffset;
	INT iblCbvSrvOffsetEnd;

	ID3D12RootSignature* GetRootSignature() { return iblRootSignature.Get(); }
	ID3D12RootSignature* GetGenBRDFLUTRootSignature() { return genBRDFLUTRootSignature.Get(); }

private:
	ComPtr<ID3D12RootSignature> genBRDFLUTRootSignature;
	ComPtr<ID3D12RootSignature> iblRootSignature;
	ComPtr<ID3D12PipelineState> genBRDFLUTState;
	ComPtr<ID3D12PipelineState> iblState;
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE genBRDFLUTRtvHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE genBRDFLUTSrvHandle;
	ComPtr<ID3D12Resource> brdfLUTRenderTarget;

	void GenerateBRDFLUT(ID3D12GraphicsCommandList* commandList);
};