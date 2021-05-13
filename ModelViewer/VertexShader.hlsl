cbuffer Camera : register(b0)
{
    float4x4 gView;
    float4x4 gProjection;
    float4x4 gViewProjection;
    float3 gEyePosW;
    float padding[13];
};

cbuffer Primitive : register(b2)
{
    float4x4 gModel;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

VSOut main(float3 position : POSITION,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 uv : TEXCOORD)
{
    VSOut result;
    float4 posW = mul(float4(position, 1.0f), gModel);
    result.positionW = posW.xyz;
    result.position = mul(posW, gViewProjection);
    result.normal = mul(normal, (float3x3)gModel);
    result.tangent = tangent;
    result.uv = uv;

    return result;
}