#ifndef _UTILS_HLSLI_
#define _UTILS_HLSLI_

LightColor CalculateLightColor(int lightIndex, float3 viewNormal, float3 viewPos)
{
    LightColor color = (LightColor)0.f;

    float3 viewLightDir = (float3)0.f;

    float diffuseRatio = 0.f;
    float specularRatio = 0.f;
    float distanceRatio = 1.f;

    if (g_light[lightIndex].lightType == 0)
    {
        // Directional Light
        //�ƿ����� ����ϴ� ��ǥ��� view��ǥ�� �̱� ������ �����ֱ� ���ؼ� view ����� �����ش�.
        viewLightDir = normalize(mul(float4(g_light[lightIndex].direction.xyz, 0.f), g_matView).xyz);
        //�������ؼ� cos��Ÿ ���� ���ϰ� saturate�� �̿��ؼ� Ȥ�ö� �Ʒ����� ���� ������ ��� -���� ���� �� �ֱ� ������ -���� 0���� �ٲ��ֱ� ���ؼ� ���
        //��¥�� cos���̱� ������ ���� -1~1 ������ ������ ���´�.
        diffuseRatio = saturate(dot(-viewLightDir, viewNormal));
    }
    else if (g_light[lightIndex].lightType == 1)
    {
        // Point Light
        float3 viewLightPos = mul(float4(g_light[lightIndex].position.xyz, 1.f), g_matView).xyz;
        //������ ���� ����
        viewLightDir = normalize(viewPos - viewLightPos);
        diffuseRatio = saturate(dot(-viewLightDir, viewNormal));

        //�Ÿ��� ���� �� ���� ���� ����
        float dist = distance(viewPos, viewLightPos);//������ ��ġ���� �ȼ������� �Ÿ��� ���Ѵ�.
        if (g_light[lightIndex].range == 0.f)
            distanceRatio = 0.f;
        else
            distanceRatio = saturate(1.f - pow(dist / g_light[lightIndex].range, 2));//�Ÿ��� �ִ� �Ÿ� �������� ���ۼ�Ʈ ���� �Դ��� ���Ѵ�. 1- �� ���� ���ν� �־������� �پ�鵵��
    }
    else
    {
        // Spot Light
        float3 viewLightPos = mul(float4(g_light[lightIndex].position.xyz, 1.f), g_matView).xyz;

        //�������� �ȼ������� ����
        viewLightDir = normalize(viewPos - viewLightPos);

        //������ �Ի簢�� ǥ���� �븻�� �̷�� ��
        diffuseRatio = saturate(dot(-viewLightDir, viewNormal));

        if (g_light[lightIndex].range == 0.f)
            distanceRatio = 0.f;
        else
        {
            //����Ʈ����Ʈ�� �ִ� ������ ����
            float halfAngle = g_light[lightIndex].angle / 2;

            //�������� �ȼ������� ����
            float3 viewLightVec = viewPos - viewLightPos;

            //����Ʈ����Ʈ�� ���� ���� ����, ���� ��ǥ�踦 �����ֱ� ���� view ����� �����ش�.
            float3 viewCenterLightDir = normalize(mul(float4(g_light[lightIndex].direction.xyz, 0.f), g_matView).xyz);

            //����Ʈ����Ʈ�� ���� ����� �������� �ȼ������� ���͸� �����Ѵ�.
            //���⼭ �������� �ȼ������� ���ʹ� �������Ͱ� �ƴϱ� ������ ��������� 
            //�ش� ���� �������� �ȼ������� ���͸� ����Ʈ����Ʈ�� ���� ���⿡ �������� �� ���̰� �ȴ�.
            //�����̱� ������ ������ ���� ���� �ִ�.
            float centerDist = dot(viewLightVec, viewCenterLightDir);
            distanceRatio = saturate(1.f - centerDist / g_light[lightIndex].range);

            //�þ߰��� �����ش�.
            //�������� �ȼ������� ���͸� �ܿ����ͷ� �ٲ��ְ� ����Ʈ����Ʈ�� ���� ���� ���� ���Ϳ� �����ؼ�
            //cos��Ÿ ���� �����ְ� �ش� ���� acos�� �̿��� ������ ����� �̷�� ���� �����ش�.
            float lightAngle = acos(dot(normalize(viewLightVec), viewCenterLightDir));

            if (centerDist < 0.f || centerDist > g_light[lightIndex].range) // �ִ� �Ÿ��� �������
                distanceRatio = 0.f;
            else if (lightAngle > halfAngle) // �ִ� �þ߰��� �������
                distanceRatio = 0.f;
            else // �Ÿ��� ���� ������ ���⸦ ����
                distanceRatio = saturate(1.f - pow(centerDist / g_light[lightIndex].range, 2));
        }
    }

    //���ݻ� ���͸� �����ִ� �κ�
    //�Ի� ���Ϳ� �븻�� �̿��ؼ� ���� ���� ������(���� �븻�������� �ö󰡾��ϴ� �Ÿ��� ������ ũ��) 
    //�븻�� �Ÿ��� ���ؼ� ���ΰ��� ���͸� ���� ������ �ι� �����༭ ���������� �ݻ纤�͸� �����ش�.
    float3 reflectionDir = normalize(viewLightDir + 2 * (saturate(dot(-viewLightDir, viewNormal)) * viewNormal));

    //�׷��� ��ü�� ��ġ���� ī�޶�� ���� ���͸� ���ؾ� �ϴµ� 
    //�ش� ��ǥ��� View ��ǥ�� �̱� ������ ������ ī�޶� �����̴�.(0,0,0)
    //���� �׳� viewPos�� ��ֶ����� �ϸ� ī�޶󿡼� ������� ���� ���Ͱ� ���´�.
    float3 eyeDir = normalize(viewPos);

    //���ݻ纤�Ϳ� ī�޶� ������ ������ ���Ѵ�.
    specularRatio = saturate(dot(-eyeDir, reflectionDir));

    //���� ����ŧ�� ���� ���� ���༭ �� ���ϰ�, ���� ���� ������ ����������� �� �ش�. (cos �׷����� ������ ���� �� �� �ִ�.)
    specularRatio = pow(specularRatio, 2);

    //���� ���� diffuse������ cos��Ÿ ��ŭ �����ְ� �Ÿ� �� �� ���� ���ҷ��� �����ش�.
    color.diffuse = g_light[lightIndex].color.diffuse * diffuseRatio * distanceRatio;
    //�� ��ü�� ambient�� �״�� ����
    color.ambient = g_light[lightIndex].color.ambient * distanceRatio;
    color.specular = g_light[lightIndex].color.specular * specularRatio * distanceRatio;

    return color;
}


#endif