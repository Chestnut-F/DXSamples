#define NUM_SAMPLES 1024u
#define PI 3.1415926536

// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 Hammersley2d(uint i, uint N)
{
	// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
	uint bits = (i << 16u) | (i >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float rdi = float(bits) * 2.3283064365386963e-10;
	return float2(float(i) / float(N), rdi);
}

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 normal)
{
	float alpha = roughness * roughness;

	// Maps a 2D point to a hemisphere with spread based on roughness
	float phi = 2 * PI * Xi.x;
	float cosTheta = sqrt((1 - Xi.y) / (1 + (alpha * alpha - 1) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

	// Tangent space
	float3 up = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangentX = normalize(cross(up, normal));
	float3 tangentY = normalize(cross(normal, tangentX));

	// Convert to world Space
	return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

// Geometric Shadowing function
float SchlicksmithGGX(float NoL, float NoV, float roughness)
{
	float k = (roughness * roughness) / 2.0;
	float GL = NoL / (NoL * (1.0 - k) + k);
	float GV = NoV / (NoV * (1.0 - k) + k);
	return GL * GV;
}

float2 BRDF(float NoV, float roughness)
{
	const float3 N = float3(0.0, 0.0, 1.0);
	float3 V = float3(sqrt(1.0 - NoV * NoV), 0.0, NoV);

	float2 LUT = float2(0.0, 0.0);
	for (uint i = 0u; i < NUM_SAMPLES; i++)
	{
		float2 Xi = Hammersley2d(i, NUM_SAMPLES);
		float3 H = ImportanceSampleGGX(Xi, roughness, N);
		float3 L = 2.0 * dot(V, H) * H - V;

		float NoL = max(dot(N, L), 0.0);
		float NoV = max(dot(N, V), 0.0);
		float VoH = max(dot(V, H), 0.0);
		float NoH = max(dot(N, H), 0.0);

		if (NoL > 0.0)
		{
			float G = SchlicksmithGGX(NoL, NoV, roughness);
			float G_Vis = (G * VoH) / (NoH * NoV);
			float Fc = pow(1 - VoH, 5);
			LUT += float2((1.0 - Fc) * G_Vis, Fc * G_Vis);
		}
	}
	return LUT / float(NUM_SAMPLES);
}

struct VSOutput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET
{
	return float4(BRDF(input.uv.x, 1.0 - input.uv.y), 0.0, 1.0);
}