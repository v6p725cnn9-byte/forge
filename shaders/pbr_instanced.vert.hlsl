cbuffer Camera : register(b0, space1)
{
    column_major float4x4 view_projection;
};

struct Input
{
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv0 : TEXCOORD3;
    float2 uv1 : TEXCOORD4;
    float4 color : TEXCOORD5;
    float4 model0 : TEXCOORD6;
    float4 model1 : TEXCOORD7;
    float4 model2 : TEXCOORD8;
    float4 model3 : TEXCOORD9;
    float4 instance_color : TEXCOORD10;
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
    // GLM stores columns; HLSL float4x4(c0,c1,c2,c3) fills rows.
    const float4x4 model = transpose(float4x4(input.model0, input.model1, input.model2, input.model3));
    const float4 world = mul(model, float4(input.position, 1.0));
    output.clip = mul(view_projection, world);
    output.world = world.xyz;
    const float3x3 model3 = (float3x3)model;
    output.normal = mul(model3, input.normal);
    output.tangent = float4(mul(model3, input.tangent.xyz), input.tangent.w);
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.color = input.color * input.instance_color;
    return output;
}
