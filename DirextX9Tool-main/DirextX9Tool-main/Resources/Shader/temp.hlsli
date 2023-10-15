//���̴��� �Ķ����� �����ִ°� TableDescriptorHeap���� ���ش�.
cbuffer TRANSFORM_PARAMS : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matView;
    row_major matrix g_matProjection;
    row_major matrix g_matWV;

    row_major/*��� ���ټ����� ���̴��� �ٷ��̶� �ٸ��� ������ ������ �����ֱ� ���ؼ�*/  matrix g_matWVP;
};


struct VS_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct VS_OUT
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float3 viewPos : POSITION;
    float3 viewNormal : NORMAL;
};


//���� ���̴�
VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output = (VS_OUT)0;

    output.pos = mul(float4(input.pos, 1.f), g_matWVP);
    //output.color = input.color;
    output.uv = input.uv;

    output.viewPos = mul(float4(input.pos, 1.f), g_matWV).xyz;//�� ���꿡 ���� ���ؼ� �������� �ܰ踦 �ϱ� ������ ���� �����ش�.
    output.viewNormal = normalize(mul(float4(input.normal, 0.f), g_matWV).xyz);

    return output;
}

//�ȼ� ���̴�
float4 PS_Main(VS_OUT input) : SV_Target
{
    //�ν��ĸ� ����
    float4 color = g_tex_0.Sample(g_sam_0, input.uv);
    //float4 color = float4(1.f,1.f,1.f,1.f);

    LightColor totalColor = (LightColor)0.f;

    //������ �ȼ����� �������� �÷����� ���ؼ� �������ش�.
    for (int i = 0; i < g_lightCount; ++i)
    {
        LightColor color = CalculateLightColor(i, input.viewNormal, input.viewPos);
        totalColor.diffuse += color.diffuse;
        totalColor.ambient += color.ambient;
        totalColor.specular += color.specular;
    }

    color.xyz = (totalColor.diffuse.xyz * color.xyz)
        + totalColor.ambient.xyz * color.xyz
        + totalColor.specular.xyz;

    return color;
}


