//셰이더의 파람들을 보내주는건 TableDescriptorHeap에서 해준다.
cbuffer TRANSFORM_PARAMS : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matView;
    row_major matrix g_matProjection;
    row_major matrix g_matWV;

    row_major/*행렬 접근순서가 셰이더랑 다렉이랑 다르기 때문에 순서를 맞춰주기 위해서*/  matrix g_matWVP;
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


//정점 셰이더
VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output = (VS_OUT)0;

    output.pos = mul(float4(input.pos, 1.f), g_matWVP);
    //output.color = input.color;
    output.uv = input.uv;

    output.viewPos = mul(float4(input.pos, 1.f), g_matWV).xyz;//빛 연산에 쓰기 위해서 프로젝션 단계를 하기 이전의 값을 보내준다.
    output.viewNormal = normalize(mul(float4(input.normal, 0.f), g_matWV).xyz);

    return output;
}

//픽셀 셰이더
float4 PS_Main(VS_OUT input) : SV_Target
{
    //턱스쳐를 설정
    float4 color = g_tex_0.Sample(g_sam_0, input.uv);
    //float4 color = float4(1.f,1.f,1.f,1.f);

    LightColor totalColor = (LightColor)0.f;

    //각각의 픽셀마다 빛에대한 컬러들을 구해서 세팅해준다.
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


