static const float PI = 3.1415926536;

cbuffer Camera : register(b0)
{
    float4x4 gView;
    float4x4 gProjection;
    float4x4 gViewProjection;
    float3 gEyePosW;
    float gNearPlane;
    float gFarPlane;
};

cbuffer Material : register(b3)
{
    float4 gBaseColorFactor;
    float gMetallicFactor;
    float gRoughnessFactor;
    float gNormalScale;
    float gOcclusionStrength;
    float3 gEmissiveFactor;
};

Texture2D albedoMapTexture : register(t0);
Texture2D metallicRoughnessTexture : register(t1);
Texture2D emissiveTexture : register(t2);
Texture2D normalMapTexture : register(t3);
Texture2D occlusionTexture : register(t4);
Texture2D brdflutTexture : register(t5);
TextureCube prefilteredMapTexture : register(t6);
TextureCube irradianceTexture : register(t7);

SamplerState defaultSampler : register(s0);
SamplerState brdflutSampler : register(s1);

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 position : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 tangent : SV_TARGET2;
    float4 albedo : SV_TARGET3;
};

float3 PrefilteredReflection(float3 R, float roughness)
{
    const float MAX_REFLECTION_LOD = 10.0; // todo: param/const
    float lod = roughness * MAX_REFLECTION_LOD;
    float lodf = floor(lod);
    float lodc = ceil(lod);
    float3 a = prefilteredMapTexture.SampleLevel(defaultSampler, R, lodf).rgb;
    float3 b = prefilteredMapTexture.SampleLevel(defaultSampler, R, lodc).rgb;
    return lerp(a, b, lod - lodf);
}

float3 FresnelSchlick(float3 F0, float cosTheta)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 CalculateNormal(VSOutput input)
{
    float3 tangentNormal = normalMapTexture.Sample(defaultSampler, input.uv).xyz * 2.0 - 1.0;

    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent);
    float3 B = normalize(cross(N, T));
    float3x3 TBN = transpose(float3x3(T, B, N));

    return normalize(mul(tangentNormal, TBN));
}

PSOutput main(VSOutput input)
{
    PSOutput output;

	float3 N = input.normal;
	float3 V = normalize(gEyePosW - input.positionW);
    float3 R = 2 * dot(V, N) * N - V;
    float NoV = max(dot(N, V), 0.0);

    float3 albedo = gBaseColorFactor.rgb * albedoMapTexture.Sample(defaultSampler, input.uv).rgb;
    float metallic = gMetallicFactor * metallicRoughnessTexture.Sample(defaultSampler, input.uv).b;
    float roughness = gRoughnessFactor * metallicRoughnessTexture.Sample(defaultSampler, input.uv).g;
    float3 emissive = gEmissiveFactor * emissiveTexture.Sample(defaultSampler, input.uv).rgb;
    float occlusion = gOcclusionStrength * occlusionTexture.Sample(defaultSampler, input.uv).r;

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);
    float3 F = FresnelSchlick(F0, NoV);
    float3 kd = lerp(1.0 - F, 0.0, metallic);

    float2 brdf = brdflutTexture.Sample(brdflutSampler, float2(NoV, roughness)).rg;
    float3 reflection = PrefilteredReflection(R, roughness).rgb;
    float3 irradiance = irradianceTexture.Sample(defaultSampler, N).rgb;

    float3 diffuse = kd * irradiance * albedo;
    float3 specular = reflection * (F * brdf.x + brdf.y);
    float3 ambient = emissive + diffuse + specular;

    output.albedo = float4(occlusion * ambient, 1.0);
    return output;
}