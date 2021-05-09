
#define MaxLights 3

struct Light
{
    float3 color;
    float radius;
    float3 pos;
};

struct Material
{
    float4 diffuseFactor;
};

float CalcAttenuation(float d, float radius)
{
	return saturate((radius - d) / radius);
}

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = 1.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;

    return (mat.diffuseFactor.rgb + roughnessFactor) * lightStrength;
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for point lights.
//---------------------------------------------------------------------------------------
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    // The vector from the surface to the light.
    float3 lightVec = L.pos - pos;

    // The distance from surface to light.
    float d = length(lightVec);

    // Range test.
    if (d > L.radius)
        return 0.0f;

    // Normalize the light vector.
    lightVec /= d;

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.color * ndotl;

    // Attenuate light by distance.
    float att = CalcAttenuation(d, L.radius);
    lightStrength *= att;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}