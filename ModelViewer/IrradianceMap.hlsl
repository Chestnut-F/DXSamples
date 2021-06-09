TextureCube gInputTexture : register(t0);
RWTexture2DArray<float4> gOutputTexture : register(u0);
SamplerState gSampler : register(s0);

cbuffer PrefilterEnvMap : register(b0)
{
	float gDeltaPhi;
	float gDeltaTheta;
};

#define PI 3.1415926535897932384626433832795

float3 GetSamplingVector(uint3 ThreadID)
{
	float outputWidth, outputHeight, outputDepth;
	gOutputTexture.GetDimensions(outputWidth, outputHeight, outputDepth);

	float2 st = ThreadID.xy / float2(outputWidth, outputHeight);
	float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - 1.0;

	// Select vector based on cubemap face index.
	float3 ret = float3(0.0, 0.0, 0.0);
	switch (ThreadID.z)
	{
	case 0: ret = float3(1.0, uv.y, -uv.x); break;
	case 1: ret = float3(-1.0, uv.y, uv.x); break;
	case 2: ret = float3(uv.x, 1.0, -uv.y); break;
	case 3: ret = float3(uv.x, -1.0, uv.y); break;
	case 4: ret = float3(uv.x, uv.y, 1.0); break;
	case 5: ret = float3(-uv.x, uv.y, -1.0); break;
	}
	return normalize(ret);
}

[numthreads(32, 32, 1)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
	float3 N = GetSamplingVector(ThreadID);
	float3 up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(0.0, 0.0, 1.0);
	float3 right = normalize(cross(up, N));
	up = cross(N, right);

	const float TWO_PI = PI * 2.0;
	const float HALF_PI = PI * 0.5;

	float3 color = float3(0.0, 0.0, 0.0);
	uint sampleCount = 0u;
	for (float phi = 0.0; phi < TWO_PI; phi += gDeltaPhi) {
		for (float theta = 0.0; theta < HALF_PI; theta += gDeltaTheta) {
			float3 tempVec = cos(phi) * right + sin(phi) * up;
			float3 sampleVector = cos(theta) * N + sin(theta) * tempVec;
			color += gInputTexture.SampleLevel(gSampler, sampleVector, 0).rgb * cos(theta) * sin(theta);
			sampleCount++;
		}
	}

	gOutputTexture[ThreadID] = float4(PI * color / float(sampleCount), 1.0);
}