SamplerState  g_sampler : register(s0);
Texture2D g_txPositionDepth : register(t0);
Texture2D g_txNormal : register(t1);
Texture2D g_txTangent : register(t2);
Texture2D g_txSSAONoise : register(t3);

#define SSAO_KERNEL_SIZE 64
#define SSAO_RADIUS 0.5

cbuffer Camera : register(b0)
{
	float4x4 gView;
	float4x4 gProjection;
	float4x4 gViewProjection;
	float3 gEyePosW;
	float gNearPlane;
	float gFarPlane;
};

cbuffer SSAOKernel : register(b1)
{
	float4 gSSAOKernel[SSAO_KERNEL_SIZE];
};

struct VSOutput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET
{
	float3 pixPos = g_txPositionDepth.Sample(g_sampler, input.uv).rgb;
	float3 pixNormal = normalize(g_txNormal.Sample(g_sampler, input.uv).rgb * 2.0 - 1.0);

	float3 randomVec = g_txSSAONoise.Sample(g_sampler, input.uv).xyz * 2.0 - 1.0;

	float3 tangent = normalize(randomVec - pixNormal * dot(randomVec, pixNormal)); // Gram?CSchmidt process
	float3 bitangent = normalize(cross(tangent, pixNormal));
	float3x3 TBN = float3x3(tangent, bitangent, pixNormal);

	float occlusion = 0.0f;
	for (int i = 0; i < SSAO_KERNEL_SIZE; i++)
	{
		float3 samplePos = mul(gSSAOKernel[i].xyz, TBN);
		samplePos = pixPos + samplePos * SSAO_RADIUS;

		float4 offset = float4(samplePos, 1.0f);
		offset = mul(offset, gProjection);
		offset.xyz /= offset.w;
		offset.xy = float2(offset.x * 0.5f + 0.5f, 1.0f - (offset.y * 0.5f + 0.5f));

		float sampleDepth = g_txPositionDepth.Sample(g_sampler, offset.xy).w;

		float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(pixPos.z - sampleDepth));
		occlusion += (samplePos.z >= sampleDepth ? 1.0f : 0.0f) * rangeCheck;
	}
	occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));

	return float4(occlusion, occlusion, occlusion, 1.0f);
}