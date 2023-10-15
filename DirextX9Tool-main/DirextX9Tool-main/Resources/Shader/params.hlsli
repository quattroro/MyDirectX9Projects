#ifndef _PARAMS_HLSLI_
#define _PARAMS_HLSLI_


//Light의 정보들을 그대로 받아온다.
struct LightColor
{
    float4 diffuse;
    float4 ambient;
    float4 specular;
};

struct LightInfo
{
    LightColor  color;
    float4	    position;
    float4	    direction;
    int		    lightType;
    float	    range;
    float	    angle;
    int  	    padding;
};

cbuffer GLOBAL_PARAMS : register(b0)
{
    int         g_lightCount;
    float3      g_lightPadding;
    LightInfo   g_light[50];
}

//셰이더의 파람들을 보내주는건 TableDescriptorHeap에서 해준다.
cbuffer TRANSFORM_PARAMS : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matView;
    row_major matrix g_matProjection;
    row_major matrix g_matWV;

    row_major/*행렬 접근순서가 셰이더랑 다렉이랑 다르기 때문에 순서를 맞춰주기 위해서*/  matrix g_matWVP;
};

//메테리얼과 관련된 인자들을 넘겨 받는다.
cbuffer MATERIAL_PARAMS : register(b2)
{
    int g_int_0;
    int g_int_1;
    int g_int_2;
    int g_int_3;
    int g_int_4;
    float g_float_0;
    float g_float_1;
    float g_float_2;
    float g_float_3;
    float g_float_4;
};

//메테리얼의 텍스쳐
Texture2D g_tex_0 : register(t0);
Texture2D g_tex_1 : register(t1);
Texture2D g_tex_2 : register(t2);
Texture2D g_tex_3 : register(t3);
Texture2D g_tex_4 : register(t4);

SamplerState g_sam_0 : register(s0);

#endif 
