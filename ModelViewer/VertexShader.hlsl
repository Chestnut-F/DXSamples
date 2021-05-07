cbuffer Globals : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VSOut
{
    float4 position : SV_POSITION;
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
    result.normal = normal;
    result.tangent = tangent;
    result.uv = uv;

    return result;
}