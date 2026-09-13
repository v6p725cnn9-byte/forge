cbuffer Tonemap : register(b0, space3)
{
    float4 exposure_bloom; // x exposure, y bloom strength
};

Texture2D hdr_tex : register(t0, space2);
SamplerState hdr_s : register(s0, space2);
Texture2D bloom_tex : register(t1, space2);
SamplerState bloom_s : register(s1, space2);

struct Input
{
    float2 uv : TEXCOORD0;
};

float3 aces(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(Input input) : SV_Target0
{
    const float3 hdr = hdr_tex.Sample(hdr_s, input.uv).rgb;
    const float3 bloom = bloom_tex.Sample(bloom_s, input.uv).rgb;
    const float3 color = (hdr + bloom * exposure_bloom.y) * max(exposure_bloom.x, 0.001);
    return float4(aces(color), 1.0);
}
