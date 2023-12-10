

float4x4 matWorldViewProj;
float4x4 matWorld;
float4x4 matView;
float4x4 matProjection;

sampler2D fontTexture;
float4 baseColor;
float effect;
float lowThreshold;
float highThreshold;
float smoothing;
float borderColor;

struct VS_INPUT
{
	float4 position : POSITION0;
	float2 texcoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 position : POSITION0;
	float2 texcoord : TEXCOORD0;
};

VS_OUTPUT vs_main(VS_INPUT Input)
{
	VS_OUTPUT Output;

	Output.Position = mul(Input.Position, matViewProjection);

	return(Output);

}




struct PS_OUTPUT
{
	float4 color : COLOR0;

};

PS_OUT ps_main(VS_OUTPUT Input)
{
	PS_OUTPUT Output;

	if (effect < 1)
	{
		color.rgb = baseColor;
		color.a = tex2D(fontTexture, Input.texcoord).a;
	}
	else if (effect < 2)
	{
		// Softened edge.

		color.rgb = baseColor;
		color.a = smoothstep(0.5 - smoothing,0.5 + smoothing, tex2D(fontTexture, Input.texcoord).a);
	}
	else if (effect < 3)
	{

	}
	else if (effect < 4)
	{

	}
	else if (effect < 5)
	{

	}
	else if (effect < 6)
	{

	}

   return(Output);

}


