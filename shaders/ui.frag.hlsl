Texture2D atlas : register(t0, space2);
SamplerState atlas_sampler : register(s0, space2);

struct Input
{
    float4 clip : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float4 outline : TEXCOORD2;
    float4 glow : TEXCOORD3;
    // x: outline outer edge, y: AA width, z: glow outer edge, w: glow strength.
    // Distances in normalized SDF units, glyph edge at 0.5.
    float4 fx : TEXCOORD4;
};

float4 main(Input input) : SV_Target0
{
    const float distance = atlas.Sample(atlas_sampler, input.uv).r;
    const float aa = input.fx.y;
    const float face = smoothstep(0.5 - aa, 0.5 + aa, distance);
    const float body = smoothstep(input.fx.x - aa, input.fx.x + aa, distance);
    const float glow_band = 1.0 - smoothstep(input.fx.z, 0.5, distance);
    const float glow = glow_band * input.fx.w * input.glow.a;
    const float3 rgb = lerp(input.outline.rgb, input.color.rgb, face) + input.glow.rgb * glow;
    const float alpha = max(lerp(input.outline.a, input.color.a, face) * body, glow);
    return float4(rgb, alpha);
}
