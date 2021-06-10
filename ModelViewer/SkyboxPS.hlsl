TextureCube envTexture : register(t0);
SamplerState defaultSampler : register(s0);

struct VSOutput
{
	float4 pos : SV_POSITION;
	float3 uvw : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET
{
	float3 color = envTexture.Sample(defaultSampler, input.uvw).rgb;

	return float4(color, 1.0);
}