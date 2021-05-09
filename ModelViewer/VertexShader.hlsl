cbuffer g_constant : register(b1)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;
    float3 gEyePosW;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float3 posW : POSITION;
    float3 normal: NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

VSOut main(float3 position : POSITION,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 uv : TEXCOORD)
{
    VSOut result;

    result.position = mul(float4(position, 1.0f), gWorldViewProj);
    float4 posW = mul(float4(position, 1.0f), gWorld);
    result.posW = posW.xyz;

    result.normal = mul(normal, (float3x3)gWorld);

    result.tangent = tangent;
    result.uv = uv;

    return result;
}