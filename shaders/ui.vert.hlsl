struct Input
{
    float2 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
    float4 outline : TEXCOORD3;
    float4 glow : TEXCOORD4;
    float4 fx : TEXCOORD5;
    float4 shape : TEXCOORD6;
    float4 shape2 : TEXCOORD7;
};

struct Output
{
    float4 clip : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float4 outline : TEXCOORD2;
    float4 glow : TEXCOORD3;
    float4 fx : TEXCOORD4;
    float4 shape : TEXCOORD5;
    float4 shape2 : TEXCOORD6;
};

Output main(Input input)
{
    Output output;
    output.clip = float4(input.position, 0.0, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    output.outline = input.outline;
    output.glow = input.glow;
    output.fx = input.fx;
    output.shape = input.shape;
    output.shape2 = input.shape2;
    return output;
}
