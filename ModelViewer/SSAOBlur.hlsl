Texture2D g_txSSAO : register(t0);
SamplerState g_ssaoSampler : register(s0);

struct VSOutput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET
{
	const int blurRange = 2;
	int n = 0;
	int2 texDim;
	g_txSSAO.GetDimensions(texDim.x, texDim.y);
	float2 texelSize = 1.0 / (float2)texDim;
	float result = 0.0;
	for (int x = -blurRange; x <= blurRange; x++)
	{
		for (int y = -blurRange; y <= blurRange; y++)
		{
			float2 offset = float2(float(x), float(y)) * texelSize;
			result += g_txSSAO.Sample(g_ssaoSampler, input.uv + offset).r;
			n++;
		}
	}
	return result / (float(n));
}