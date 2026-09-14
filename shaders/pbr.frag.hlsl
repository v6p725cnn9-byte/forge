cbuffer Shading : register(b0, space3)
{
    float4 camera_pos_mips;
    float4 light_dir_ibl;
    float4 light_color_exposure;
    float4 base_color_factor;
    float4 emissive_metallic;
    float4 params;
    float4 flags;
    float4 tex_u[5];
    float4 tex_v[5];
    float4 cascade_splits;
    float4 shadow_params;
    float4 fog_color_density;
    column_major float4x4 light_vp0;
    column_major float4x4 light_vp1;
    column_major float4x4 light_vp2;
};

Texture2D albedo_tex : register(t0, space2);
SamplerState albedo_s : register(s0, space2);
Texture2D mr_tex : register(t1, space2);
SamplerState mr_s : register(s1, space2);
Texture2D normal_tex : register(t2, space2);
SamplerState normal_s : register(s2, space2);
Texture2D occlusion_tex : register(t3, space2);
SamplerState occlusion_s : register(s3, space2);
Texture2D emissive_tex : register(t4, space2);
SamplerState emissive_s : register(s4, space2);
TextureCube irradiance_tex : register(t5, space2);
SamplerState irradiance_s : register(s5, space2);
TextureCube specular_tex : register(t6, space2);
SamplerState specular_s : register(s6, space2);
Texture2D brdf_tex : register(t7, space2);
SamplerState brdf_s : register(s7, space2);
Texture2DArray shadow_map : register(t8, space2);
SamplerState shadow_s : register(s8, space2);

struct Input
{
    float3 world : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv0 : TEXCOORD3;
    float2 uv1 : TEXCOORD4;
    float4 color : TEXCOORD5;
};

static const float PI = 3.14159265;

float2 transform_uv(float2 uv0, float2 uv1, float4 u, float4 v)
{
    const float2 uv = u.w > 0.5 ? uv1 : uv0;
    return float2(dot(u.xyz, float3(uv, 1.0)), dot(v.xyz, float3(uv, 1.0)));
}

float D_GGX(float NoH, float a)
{
    const float a2 = a * a;
    const float d = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * d * d);
}

float V_SmithGGX(float NoV, float NoL, float a)
{
    const float a2 = a * a;
    const float ggxL = NoV * sqrt(max(NoL * NoL * (1.0 - a2) + a2, 1e-7));
    const float ggxV = NoL * sqrt(max(NoV * NoV * (1.0 - a2) + a2, 1e-7));
    return 0.5 / max(ggxL + ggxV, 1e-5);
}

float3 F_Schlick(float3 f0, float VoH)
{
    const float f = pow(saturate(1.0 - VoH), 5.0);
    return f0 + (1.0 - f0) * f;
}

float3 F_Schlick_roughness(float3 f0, float NoV, float roughness)
{
    const float f = pow(saturate(1.0 - NoV), 5.0);
    return f0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), f0) - f0) * f;
}

float shadow_at(float3 world)
{
    if (shadow_params.w < 0.5) return 1.0;
    const float depth = length(world - camera_pos_mips.xyz);
    int cascade = 2;
    if (depth < cascade_splits.x) cascade = 0;
    else if (depth < cascade_splits.y) cascade = 1;
    column_major float4x4 vp = light_vp0;
    if (cascade == 1) vp = light_vp1;
    if (cascade == 2) vp = light_vp2;
    const float4 light = mul(vp, float4(world, 1.0));
    const float3 proj = light.xyz / max(light.w, 1e-6);
    const float2 uv = proj.xy * float2(0.5, -0.5) + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || proj.z < 0.0 || proj.z > 1.0) return 1.0;
    float shadow = 0.0;
    const float texel = shadow_params.x;
    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            const float closest = shadow_map.Sample(shadow_s, float3(uv + float2(x, y) * texel, cascade)).r;
            shadow += closest + shadow_params.y >= proj.z ? 1.0 : 0.0;
        }
    }
    return lerp(1.0, shadow / 9.0, shadow_params.z);
}

float4 main(Input input) : SV_Target0
{
    const float2 uv_albedo = transform_uv(input.uv0, input.uv1, tex_u[0], tex_v[0]);
    const float2 uv_mr = transform_uv(input.uv0, input.uv1, tex_u[1], tex_v[1]);
    const float2 uv_normal = transform_uv(input.uv0, input.uv1, tex_u[2], tex_v[2]);
    const float2 uv_ao = transform_uv(input.uv0, input.uv1, tex_u[3], tex_v[3]);
    const float2 uv_emissive = transform_uv(input.uv0, input.uv1, tex_u[4], tex_v[4]);

    float4 albedo = albedo_tex.Sample(albedo_s, uv_albedo) * base_color_factor * input.color;
    if (flags.y > 0.5 && albedo.a < params.w) discard;

    const float3 orm = mr_tex.Sample(mr_s, uv_mr).rgb;
    const float occlusion = lerp(1.0, occlusion_tex.Sample(occlusion_s, uv_ao).r * orm.r, params.z);
    const float roughness = saturate(orm.g * params.x);
    const float metallic = saturate(orm.b * emissive_metallic.w);
    const float3 emissive = emissive_tex.Sample(emissive_s, uv_emissive).rgb * emissive_metallic.xyz;

    float3 N = normalize(input.normal);
    const float3 T = normalize(input.tangent.xyz);
    const float3 B = cross(N, T) * input.tangent.w;
    float3 n_ts = normal_tex.Sample(normal_s, uv_normal).xyz * 2.0 - 1.0;
    n_ts.xy *= params.y;
    N = normalize(T * n_ts.x + B * n_ts.y + N * n_ts.z);

    const float3 V = normalize(camera_pos_mips.xyz - input.world);
    const float NoV = saturate(dot(N, V));
    const float3 R = reflect(-V, N);
    const float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo.rgb, metallic);
    const float3 diffuse_color = albedo.rgb * (1.0 - metallic);

    if (flags.x > 0.5) {
        return float4(albedo.rgb + emissive, 1.0);
    }

    const float3 L = normalize(light_dir_ibl.xyz);
    const float NoL = saturate(dot(N, L));
    const float3 H = normalize(V + L);
    const float a = max(roughness * roughness, 0.002);
    float3 direct = float3(0.0, 0.0, 0.0);
    if (NoL > 0.0) {
        const float3 F = F_Schlick(f0, saturate(dot(V, H)));
        const float3 spec = D_GGX(saturate(dot(N, H)), a) * V_SmithGGX(NoV, NoL, a) * F;
        const float3 diffuse = diffuse_color / PI;
        direct = (diffuse * (1.0 - F) + spec) * light_color_exposure.xyz * NoL * shadow_at(input.world);
    }

    const float3 irradiance = irradiance_tex.Sample(irradiance_s, N).rgb;
    const float mip = roughness * max(camera_pos_mips.w - 1.0, 0.0);
    const float3 prefiltered = specular_tex.SampleLevel(specular_s, R, mip).rgb;
    const float2 brdf = brdf_tex.Sample(brdf_s, float2(NoV, roughness)).rg;
    const float3 F_ibl = F_Schlick_roughness(f0, NoV, roughness);
    const float3 specular = prefiltered * (F_ibl * brdf.x + brdf.y);
    const float3 indirect = (diffuse_color * irradiance + specular) * occlusion * light_dir_ibl.w;

    // Exp2 distance haze. Unlit (sky) returns above and stays clear.
    const float fog_dist = length(input.world - camera_pos_mips.xyz);
    const float fog_amount = 1.0 - exp(-fog_color_density.w * fog_color_density.w * fog_dist * fog_dist);
    const float3 shaded = direct + indirect + emissive;
    return float4(shaded * (1.0 - fog_amount) + fog_color_density.rgb * fog_amount, 1.0);
}
