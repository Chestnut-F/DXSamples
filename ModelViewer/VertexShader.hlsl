cbuffer g_constant : register(b1)
{
    float4x4 gWorldViewProj;
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
    result.posW = position;
    result.normal = normal;
    result.tangent = tangent;
    result.uv = uv;

    return result;
}