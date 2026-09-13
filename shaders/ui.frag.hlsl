Texture2D atlas : register(t0, space2);
SamplerState atlas_sampler : register(s0, space2);

struct Input
{
    float4 clip : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
};

float4 main(Input input) : SV_Target0
{
    const float alpha = atlas.Sample(atlas_sampler, input.uv).r;
    return float4(input.color.rgb, input.color.a * alpha);
}
