#include "pch.h"
#include "Shader.h"

extern LPDIRECT3DDEVICE9   g_pd3dDevice;

Shader::Shader():Object(OBJECT_TYPE::SHADER)
{
	



}

Shader::~Shader()
{
}

void Shader::Init(const wstring& path)
{
	//D3DVERTEXELEMENT9	decl[3];
	// FVF�� ����ؼ� ���������� �ڵ����� ä���ִ´�
	//D3DXDeclaratorFromFVF(D3DFVF_CUSTOMVERTEX, decl);
	// ������������ g_pDecl�� �����Ѵ�.
	//g_pd3dDevice->CreateVertexDeclaration(decl, &g_pDecl);

	//�ϳ��� ��ġ���Ϳ� 3���� ��������
	/*D3DVERTEXELEMENT9 decl[]
	{
		{0,0,D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,0},
		{0,12,D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,0},
		{0,20,D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,0},
		{0,32,D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT,0},
		D3DDECL_END()
	};

	IDirect3DVertexDeclaration9* _decl = 0;
	HRESULT hr = g_pd3dDevice->CreateVertexDeclaration(decl, &_decl);
	g_pd3dDevice->SetVertexDeclaration(_decl);*/


	CreateVertexShader(path, "VS_Main", "vs_2_0");
	//CreatePixelShader(path, "PS_Main", "ps_2_0");
}



void Shader::CreateVertexShader(const wstring& path, const string& name, const string& version)
{
	//GetDeviceCaps()
	LPD3DXBUFFER buff = NULL;
	LPD3DXBUFFER err = NULL;
	DWORD dwFlags = 0;
	//LPD3DXCONSTANTTABLE pTbv;//�ش� ���̴��� ������Ʈ ���̺��� �޾ƿ´�.

	//LPD3DXCONSTANTTABLE pTbv
	//�ش� �Լ��� ���̴��� �о����
	HRESULT r = D3DXCompileShaderFromFile(
		"TestVertex.txt"
		, NULL
		, NULL
		, "VS_Main"/*������ �Լ�*/
		, "vs_2_0"/*����*/
		, dwFlags//������ �÷���
		, &buff//�����ϵ� ���̴� �ڵ带 ������ ID3DXBUFFER������
		, &err//�����޽��� ������ ID3DXBUFFER������
		, &pTbv//���̴��� ��� ���̺� ������
	);

	if(err)
	{
		::MessageBox(0, (char*)err->GetBufferPointer(), 0, 0);
		//Release<>
	}

	if (r == D3DERR_INVALIDCALL)
	{
		int a = 0;
	}
	else if (r == D3DXERR_INVALIDDATA)
	{
		int a = 0;
	}
	else if (r == E_OUTOFMEMORY)
	{
		int a = 0;
	}

	/*���ϴ� ������ �����ϴ� �θ� ����ü�� �ĺ��ϱ� ���� D3DXHANDLE, ���� ��� Ư�� ����ü �ν��Ͻ����� ������ ��� �ڵ���
	���Ѵٸ� ����� ����ü �ν��Ͻ����� �ڵ��� �����Ѵ�. 
	���� �ֻ��� �������� �ڵ��� ����� �Ѵٸ� �� ���ڿ� 0�� �����Ѵ�.*/
	//D3DXHANDLE h0 = ConstTable->GetComstantByName(0/*����� ��ȿ ���� ���ϴ� ������ �����ϴ�  */,"������")
	//ID3DXConstantTable::setXXX("��� ���̺�� ����� ��ġ�� ������", "�����Ϸ��� ������ �����ϴ� �ڵ�", "��");
	//�迭�� ������ ������ �������� �迭�� ũ�� �Ӽ��� �߰��ȴ�.

	//g_pd3dDevice->CreateVertexDeclaration

	g_Val_Handle = pTbv->GetConstantByName(0, "VeiwProjMatrix");

	_verShader = nullptr;
	g_pd3dDevice->CreateVertexShader((DWORD*)buff->GetBufferPointer(), &_verShader);

	Release<LPD3DXBUFFER>(buff);

	//D3DXHANDLE ViewProjMatrixHandle = pTbv->GetConstantByName(0, "VeiwProjMatrix");
	//pTbv->SetMatrix(g_pd3dDevice,ViewProjMatrixHandle,)
}

void Shader::CreatePixelShader(const wstring& path, const string& name, const string& version)
{
	LPD3DXBUFFER buff = NULL;
	LPD3DXBUFFER err = NULL;
	DWORD dwFlags = 0;
	LPD3DXCONSTANTTABLE pTbv;
	//�ش� �Լ��� ���̴��� �о����
	HRESULT r = D3DXCompileShaderFromFile(
		"TestPixel.txt"
		, NULL
		, NULL
		, "PS_Main"/*������ �Լ�*/
		, "vs_2_0"/*����*/
		, dwFlags//������ �÷���
		, &buff//�����ϵ� ���̴� �ڵ带 ������ ID3DXBUFFER������
		, &err//�����޽��� ������ ID3DXBUFFER������
		, &pTbv//���̴��� ��� ���̺� ������
	);

	if (err)
	{
		::MessageBox(0, (char*)err->GetBufferPointer(), 0, 0);
		//Release<>
	}

	if (r == D3DERR_INVALIDCALL)
	{
		int a = 0;
	}
	else if (r == D3DXERR_INVALIDDATA)
	{
		int a = 0;
	}
	else if (r == E_OUTOFMEMORY)
	{
		int a = 0;
	}

	
	g_pd3dDevice->CreatePixelShader((DWORD*)buff->GetBufferPointer(), &_pixShader);
	//g_pd3dDevice->SetPixelShader(_pixShader);

}





void Shader::Update()
{
}

void Shader::CreateShader()
{
	LPD3DXBUFFER buff = NULL;
	LPD3DXBUFFER err = NULL;
	DWORD dwFlags = 0;
	LPD3DXCONSTANTTABLE pTbv;
	//�ش� �Լ��� ���̴��� �о����
	D3DXCompileShaderFromFile(
		"../Resources/shader/Test.hlsli"
		, NULL
		, NULL
		,/*������ �Լ�*/"VS_Main"
		,/*����*/"vs_5_0"
		, dwFlags//������ �÷���
		, &buff//�����ϵ� ���̴� �ڵ带 ������ ID3DXBUFFER������
		, &err//�����޽��� ������ ID3DXBUFFER������
		, &pTbv//���̴��� ��� ���̺� ������
	);
	//D3DXCompileShaderFromFile
	//d3dxCompile
	LPDIRECT3DVERTEXSHADER9 p_shader;
	g_pd3dDevice->CreateVertexShader((DWORD*)buff->GetBufferPointer(), &p_shader);

	//D3DVERTEXELEMENT9 vertecelement=
	//{
	//	/*���ؽ� ��Ұ� ����� ��Ʈ���� �����Ѵ�.*/,
	//	/*���ؽ� ����ü ���� ���ؽ� ��ҷ��� ������ ����Ű�� ����Ʈ ������ ������, ���� ��� 
	//	������ ���� ���ؽ� ����ü�� �ִٸ� (vector3 pos, vector3 normal) pos�� 0 normal�� 12 �� �ȴ�.*/,
	//	/*������ ���� �����Ѵ�. D3DDECLTYPE �������� ��� �� � ���̳� �� �� �ִ�.*/
	//	/*�ε� �Ҽ��� ��Į��*/
	//	/*2D �ε� �Ҽ��� ����*/
	//	/*3D �ε� �Ҽ��� ����*/
	//	/*4D �ε� �Ҽ��� ����*/
	//	/*Į��*/,
	//	/*����ȭ(�׼����̼�) ����� �����Ѵ�.*/,
	//	/*���ؽ� ����� ��ȹ�� �뵵�� �����Ѵ�. (��ġ����, ��������ǥ ���)*/,
	//	/*���ؽ� ����Ʈ�� ũ�⸦ ����, */,
	//	/*������ �뵵 ������ �ټ��� ���ؽ� ��Ҹ� �ĺ��ϴµ� �̿�*/
	//}

	//�ϳ��� ��ġ���Ϳ� 3���� ��������
	D3DVERTEXELEMENT9 decl[]
	{
		{0,0,D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,0},
		{0,12,D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,0},
		{0,24,D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,1},
		{0,36,D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,2},
		D3DDECL_END()
	};

	IDirect3DVertexDeclaration9* _decl = 0;
	HRESULT hr = g_pd3dDevice->CreateVertexDeclaration(decl, &_decl);
	g_pd3dDevice->SetVertexDeclaration(_decl); // SetFVF ��� ��� set fvf�� ���������� Decl�� �����ȴ�.

	/*���ؽ� ���̴� �Է� ����ü�� ������ ����� �����ϴ� ���� �����ϴ� ����� �ȴ�*/
	//���� ���̴� �ڵ忡���� �Է� ����ü
	/*struct VS_INPUT
	{
		vector position : POSITION;
		vector normal : NORMAL0;
		vector faceNormal : NORMAL1;
		vector faceNormal : NORMAL2;
	};
	���  0�� decl�̰� �뵵 POSITION�� �뵵 �ε��� 0���� �ĺ��Ǹ� position���� ���εȴ�.
	��� 1�� decl�̰� �뵵 NORMAL�� �뵵 �ε��� 0���� �ĺ��Ǹ� normal�� ���εȴ�.
	��� 2�� decl�̰� �뵵 NORMAL�� �뵵 �ε��� 1�� �ĺ��Ǹ� faceNormal1�� ���εȴ�.....*/

	/*���ؽ� �Է� �뵵��
	POSITION -> ��ġ
	BLENDWEIGHTS -> ���� ����ġ
	BLENDINDICES -> ���� �ε���
	NORMAL -> ���� ����
	PSIZE -> ���ؽ� ����Ʈ ũ��
	DIFFUSE -> ���ݻ� �÷�
	SPECULAR -> ���ݻ� �÷�
	TEXCOORD -> �ؽ��� ��ǥ
	TANGENT -> ���� ����
	BINORMAL -> ���߹��� ����
	TESSFACTOR -> ����ȭ �μ�*/

	/*���ؽ� ��� �뵵��
	POSITION -> ��ġ
	PSIZE -> ���ؽ� ����Ʈ ũ��
	FOG -> �Ȱ� ���� ��
	COLOR -> ���ؽ� �÷�, �ټ��� ���ؽ� �÷��� ��� �� �� �ִٴ� �� �����Ұ� �� �÷��� ����Ǿ� ���� �÷��� ����� ����.
	TEXCOORD -> ���ؽ� �ؽ��� ��ǥ, �ټ��� �ؽ��� ��ǥ�� ����� �� �ִٴ� �� ����*/

	/*1. ���ؽ� ���̴��� �ۼ��ϰ� ������ �Ѵ�.
	  2. �����ϵ� ���̴� �ڵ带 ������� ���ؽ� ���̴��� ��Ÿ���� IDirect3DVertexShader9 �������̽��� ���簡.
	  3. IDirect3DDevice9::SetVertexShader �޼��带 �̿��� ���ؽ� ���̴��� Ȱ��ȭ �Ѵ�.*/

	  //�̿��� ���� �ڿ��� Release�� �̿��� ���̴��� �����ؾ� �Ѵ�.
	  //d3d::Release<IDirec>


}