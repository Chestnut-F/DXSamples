cbuffer Camera : register(b0)
{
	float4x4 gView;
	float4x4 gProjection;
	float4x4 gViewProjection;
	float3 gEyePosW;
	float gNearPlane;
	float gFarPlane;
};

static const float3 gTexCoords[36] =
{
    float3(-10.0f,  10.0f, -10.0f),
    float3(-10.0f, -10.0f, -10.0f),
    float3( 10.0f, -10.0f, -10.0f),
    float3(10.0f, -10.0f, -10.0f),
    float3(10.0f,  10.0f, -10.0f),
    float3(-10.0f,  10.0f, -10.0f),

    float3(-10.0f, -10.0f,  10.0f),
    float3(-10.0f, -10.0f, -10.0f),
    float3(-10.0f,  10.0f, -10.0f),
    float3(-10.0f,  10.0f, -10.0f),
    float3(-10.0f,  10.0f,  10.0f),
    float3(-10.0f, -10.0f,  10.0f),

    float3(10.0f, -10.0f, -10.0f),
    float3(10.0f, -10.0f,  10.0f),
    float3(10.0f,  10.0f,  10.0f),
    float3(10.0f,  10.0f,  10.0f),
    float3(10.0f,  10.0f, -10.0f),
    float3(10.0f, -10.0f, -10.0f),

    float3(-10.0f, -10.0f,  10.0f),
    float3(-10.0f,  10.0f,  10.0f),
    float3(10.0f,  10.0f,  10.0f),
    float3(10.0f,  10.0f,  10.0f),
    float3(10.0f, -10.0f,  10.0f),
    float3(-10.0f, -10.0f,  10.0f),

    float3(-10.0f,  10.0f, -10.0f),
    float3(10.0f,  10.0f, -10.0f),
    float3(10.0f,  10.0f,  10.0f),
    float3(10.0f,  10.0f,  10.0f),
    float3(-10.0f,  10.0f,  10.0f),
    float3(-10.0f,  10.0f, -10.0f),

    float3(-10.0f, -10.0f, -10.0f),
    float3(-10.0f, -10.0f,  10.0f),
    float3(10.0f, -10.0f, -10.0f),
    float3(10.0f, -10.0f, -10.0f),
    float3(-10.0f, -10.0f,  10.0f),
    float3(10.0f, -10.0f,  10.0f)
};

struct VSOutput
{
	float4 pos : SV_POSITION;
	float3 uvw : TEXCOORD;
};

VSOutput main(uint vertexIndex : SV_VertexID)
{
	VSOutput output;
    output.uvw = gTexCoords[vertexIndex];
    float3 pos = gTexCoords[vertexIndex] + gEyePosW;
	output.pos = mul(float4(pos, 1.0), gViewProjection);
	return output;
}