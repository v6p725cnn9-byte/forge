cbuffer Camera : register(b0, space1)
{
    column_major float4x4 view_projection;
    column_major float4x4 model;
};

cbuffer Joints : register(b1, space1)
{
    column_major float4x4 joints[80];
};

struct Input
{
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv0 : TEXCOORD3;
    float2 uv1 : TEXCOORD4;
    float4 color : TEXCOORD5;
    float4 joint_indices : TEXCOORD6;
    float4 joint_weights : TEXCOORD7;
};

struct Output
{
    float4 clip : SV_Position;
    float3 world : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv0 : TEXCOORD3;
    float2 uv1 : TEXCOORD4;
    float4 color : TEXCOORD5;
};

Output main(Input input)
{
    Output output;
    float4x4 skin = input.joint_weights.x * joints[int(input.joint_indices.x + 0.5)]
        + input.joint_weights.y * joints[int(input.joint_indices.y + 0.5)]
        + input.joint_weights.z * joints[int(input.joint_indices.z + 0.5)]
        + input.joint_weights.w * joints[int(input.joint_indices.w + 0.5)];
    const float weight_sum =
        input.joint_weights.x + input.joint_weights.y + input.joint_weights.z + input.joint_weights.w;
    if (weight_sum < 1e-5) skin = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

    const float4 skinned = mul(skin, float4(input.position, 1.0));
    const float4 world = mul(model, skinned);
    output.clip = mul(view_projection, world);
    output.world = world.xyz;
    const float3x3 skin3 = (float3x3)skin;
    const float3x3 model3 = (float3x3)model;
    output.normal = mul(model3, mul(skin3, input.normal));
    output.tangent = float4(mul(model3, mul(skin3, input.tangent.xyz)), input.tangent.w);
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.color = input.color;
    return output;
}
