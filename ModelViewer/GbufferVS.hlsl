cbuffer Camera : register(b0)
{
    float4x4 gView;
    float4x4 gProjection;
    float4x4 gViewProjection;
    float3 gEyePosW;
    float gNearPlane;
    float gFarPlane;
};

cbuffer Primitive : register(b2)
{
    float4x4 gModel;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

VSOutput main(float3 position : POSITION,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 uv : TEXCOORD)
{
    VSOutput output;
    float4x4 modelView = mul(gModel, gView);

    float4 posW = mul(float4(position, 1.0f), modelView);
    output.positionW = posW.xyz;
    output.position = mul(posW, gProjection);

    output.normal = mul(normal, (float3x3)modelView);
    output.tangent = mul(tangent.xyz, (float3x3)modelView);
    output.uv = uv;

    return output;
}