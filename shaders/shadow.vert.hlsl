cbuffer Light : register(b0, space1)
{
    column_major float4x4 light_view_projection;
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

float4 main(Input input) : SV_Position
{
    return mul(light_view_projection, float4(input.position, 1.0));
}
