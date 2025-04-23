struct VS_INPUT {
	float4 pos: POSITION;
	float3 col: COLOR;
};

struct VS_OUTPUT {
	float4 pos: SV_POSITION;
	float3 col: COLOR;
};

cbuffer constant: register(b0)
{
	row_major float4x4 world;
	row_major float4x4 view;
	row_major float4x4 projection;
	unsigned int time;
}

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	
	output.pos = mul(input.pos, world);
	output.pos = mul(output.pos, view);
	output.pos = mul(output.pos, projection);

	output.col = input.col;
	return output;
}
