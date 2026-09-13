cbuffer Camera : register(b0, space1)
{
    column_major float4x4 view_projection;
};

struct Input
{
    float3 position : TEXCOORD0;
    float3 color : TEXCOORD1;
};

struct Output
{
    float4 position : SV_Position;
    float3 color : TEXCOORD0;
};

Output main(Input input)
{
    Output output;
    output.position = mul(view_projection, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}
