cbuffer Camera : register(b0)
{
    float4x4 gView;
    float4x4 gProjection;
    float4x4 gViewProjection;
    float3 gEyePosW;
    float padding[13];
};

cbuffer Light : register(b1)
{
    float3 Color;
    float Radius;
    float3 PositionW;
    float Area;
};

cbuffer Material : register(b3)
{
    float4 gBaseColorFactor;
};

Texture2D     g_txBaseColor : register(t0);
SamplerState  g_sampler : register(s0);

struct VSOut
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

float4 main(VSOut input) : SV_TARGET
{
    return g_txBaseColor.Sample(g_sampler, input.uv) * gBaseColorFactor;
}