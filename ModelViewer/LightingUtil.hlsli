
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

float3 BlinnPhong(float3 lightColor, float3 toLight, float3 normal, float3 toEye, Material mat)
{
    float p = 64;
    float ks = 1;

    float3 halfVec = normalize(toEye + toLight);

    float3 Ld = mat.diffuseFactor * lightColor;
    float3 Ls = ks * pow(max(0, dot(normal, halfVec)), p) * lightColor;

    return Ld + Ls;
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