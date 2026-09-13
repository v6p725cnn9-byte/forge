cbuffer Camera : register(b0, space1)
{
    column_major float4x4 view_projection;
    column_major float4x4 model;
};

struct Input
{
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv0 : TEXCOORD3;
    float2 uv1 : TEXCOORD4;
    float4 color : TEXCOORD5;
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
    const float4 world = mul(model, float4(input.position, 1.0));
    output.clip = mul(view_projection, world);
    output.world = world.xyz;
    output.normal = mul((float3x3)model, input.normal);
    output.tangent = input.tangent;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.color = input.color;
    return output;
}
