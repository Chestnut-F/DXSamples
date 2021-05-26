cbuffer Camera : register(b0)
{
    float4x4 gView;
    float4x4 gProjection;
    float4x4 gViewProjection;
    float3 gEyePosW;
    float gNearPlane;
    float gFarPlane;
};

cbuffer Light : register(b1)
{
    float3 gColor;
    float gRadius;
    float3 gPositionW;
    float gArea;
};

cbuffer Material : register(b3)
{
    float4 gBaseColorFactor;
};

Texture2D     g_txBaseColor : register(t0);
//Texture2D     g_txMetallicRoughness : register(t1);
SamplerState  g_sampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 position : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 tangent : SV_TARGET2;
    float4 albedo : SV_TARGET3;
};

float linearDepth(float depth)
{
    float z = depth * 2.0f - 1.0f;
    return (2.0f * gNearPlane * gFarPlane) / (gFarPlane + gNearPlane - z * (gFarPlane - gNearPlane));
}

PSOutput main(VSOutput input)
{
    PSOutput output;
    output.position = float4(input.positionW, linearDepth(input.position.z));
    output.normal = float4(normalize(input.normal) * 0.5 + 0.5, 1.0);
    output.tangent = float4(normalize(input.tangent.xyz) * 0.5 + 0.5, 1.0);
    output.albedo = g_txBaseColor.Sample(g_sampler, input.uv) * gBaseColorFactor;
    return output;
}