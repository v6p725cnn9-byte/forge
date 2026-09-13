cbuffer Bloom : register(b0, space3)
{
    float4 params; // x threshold, y knee
};

Texture2D hdr_tex : register(t0, space2);
SamplerState hdr_s : register(s0, space2);

struct Input
{
    float2 uv : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    const float3 color = hdr_tex.Sample(hdr_s, input.uv).rgb;
    const float lum = dot(color, float3(0.2126, 0.7152, 0.0722));
    const float soft = max(lum - params.x + params.y, 0.0);
    const float weight = max(lum - params.x, soft * soft / max(4.0 * params.y, 1e-4)) / max(lum, 1e-4);
    return float4(color * saturate(weight), 1.0);
}
