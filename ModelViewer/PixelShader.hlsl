#include "LightingUtil.hlsli"

cbuffer g_light : register(b0)
{
    Light gLight;
};

cbuffer g_constant : register(b1)
{
    float4x4 gWorldViewProj;
    float3 gEyePosW;
};

cbuffer g_material : register(b2)
{
    float4 baseColorFactor;
};

Texture2D     g_txDiffuse : register(t0);
SamplerState  g_sampler : register(s0);

struct VSOut
{
    float4 position : SV_POSITION;
    float3 posW : POSITION;
    float3 normal: NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

float4 main(VSOut input) : SV_TARGET
{
    Material mat;
    mat.diffuseFactor = g_txDiffuse.Sample(g_sampler, input.uv) * baseColorFactor;
    float3 normal = normalize(input.normal);
    float3 toEyeW = normalize(gEyePosW - input.posW);

    return float4(ComputePointLight(gLight, mat, input.posW, normal, toEyeW), 1.0);
}