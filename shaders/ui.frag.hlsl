cbuffer UiTarget : register(b0, space3)
{
    float2 target_size;
};

Texture2D atlas : register(t0, space2);
SamplerState atlas_sampler : register(s0, space2);
Texture2D backdrop : register(t1, space2);
SamplerState backdrop_sampler : register(s1, space2);

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
    // x/y: px offset inside the box, z/w: box size in px.
    float4 shape : TEXCOORD5;
    // x: corner radius, y: border width, z: box mode flag, w: softness in px.
    float4 shape2 : TEXCOORD6;
};

float box_distance(float2 local, float2 size, float radius)
{
    const float2 half_size = 0.5 * size;
    const float2 q = abs(local - half_size) - half_size + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float4 main(Input input) : SV_Target0
{
    if (input.shape2.z > 0.5) {
        const float d = box_distance(input.shape.xy, input.shape.zw, input.shape2.x);
        const float aa = 1.0 + input.shape2.w;
        const float cover = 1.0 - smoothstep(-aa, aa, d);
        const float inner = 1.0 - smoothstep(-input.shape2.y - aa, -input.shape2.y + aa, d);
        const float band = clamp(cover - inner, 0.0, 1.0);
        float3 base = input.color.rgb;
        float base_a = input.color.a;
        if (input.fx.w > 0.0) {
            // Frosted glass: blurred scene tinted by the fill color.
            const float lod = clamp(log2(max(input.fx.w * 0.25, 1.0)), 0.0, 5.0);
            const float3 blur = backdrop.SampleLevel(backdrop_sampler, input.clip.xy / target_size, lod).rgb;
            base = lerp(blur, input.color.rgb, input.color.a);
            base_a = 1.0;
        }
        const float3 rgb = lerp(input.outline.rgb, base, inner);
        const float alpha = inner * base_a + band * input.outline.a;
        return float4(rgb, alpha);
    }
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
