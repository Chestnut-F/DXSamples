Texture2D g_txAlbedo : register(t0);
Texture2D g_txSSAOBlur : register(t1);
SamplerState  g_sampler : register(s0);

struct VSOutput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET
{
	float occlusion = g_txSSAOBlur.Sample(g_sampler, input.uv).r;
	float4 albedo = g_txAlbedo.Sample(g_sampler, input.uv) * occlusion;

    return albedo;
}