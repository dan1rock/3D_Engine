struct VS_INPUT {
	float4 pos: POSITION0;
	float3 normal: NORMAL0;
	float2 texCoord: TEXCOORD0;
};

struct VS_OUTPUT {
	float4 pos: SV_POSITION0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD0;
    float3 cameraDir : TEXCOORD2;
    float3 lightDir : TEXCOORD3;
};

cbuffer constant: register(b0)
{
	row_major float4x4 world;
	row_major float4x4 model;
	row_major float4x4 view;
	row_major float4x4 projection;
    float3 cameraPos;
    float3 lightPos;
    float3 lightColor;
	unsigned int time;
}

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	
	output.pos = mul(input.pos, world);
    output.pos = mul(output.pos, model);
    output.cameraDir = normalize(output.pos.xyz - cameraPos.xyz);
    output.lightDir = normalize(lightPos.xyz - output.pos.xyz);
	output.pos = mul(output.pos, view);
	output.pos = mul(output.pos, projection);

    output.normal = normalize(mul(input.normal, world));
    output.texCoord = input.texCoord;
	return output;
}
