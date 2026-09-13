struct Input
{
    float3 color : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    return float4(input.color, 1.0);
}
