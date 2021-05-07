float4        g_txBaseColorFactor : register(b1);
Texture2D     g_txDiffuse : register(t0);
SamplerState  g_sampler : register(s0);

struct VSOut
{
    float4 position : SV_POSITION;
    float3 normal: NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

float4 main(VSOut input) : SV_TARGET
{
    return g_txDiffuse.Sample(g_sampler, input.uv) * g_txBaseColorFactor;
}