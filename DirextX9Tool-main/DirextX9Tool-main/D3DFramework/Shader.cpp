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
	// FVF를 사용해서 정점선언값을 자동으로 채워넣는다
	//D3DXDeclaratorFromFVF(D3DFVF_CUSTOMVERTEX, decl);
	// 정점선언값으로 g_pDecl을 생성한다.
	//g_pd3dDevice->CreateVertexDeclaration(decl, &g_pDecl);

	//하나의 위치벡터와 3개의 법선벡터
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
	//LPD3DXCONSTANTTABLE pTbv;//해당 셰이더의 컨스턴트 테이블을 받아온다.

	//LPD3DXCONSTANTTABLE pTbv
	//해당 함수로 셰이더를 읽어오고
	HRESULT r = D3DXCompileShaderFromFile(
		"TestVertex.txt"
		, NULL
		, NULL
		, "VS_Main"/*진입점 함수*/
		, "vs_2_0"/*버전*/
		, dwFlags//컴파일 플래그
		, &buff//컴파일된 셰이더 코드를 가지는 ID3DXBUFFER포인터
		, &err//에러메시지 가지는 ID3DXBUFFER포인터
		, &pTbv//셰이더의 상수 테이블 데이터
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

	/*원하는 변수가 존재하는 부모 구조체를 식별하기 위한 D3DXHANDLE, 예를 들어 특정 구조체 인스턴스로의 데이터 멤버 핸들을
	원한다며 여기게 구조체 인스턴스로의 핸들을 전달한다. 
	만약 최상위 변수로의 핸들을 얻고자 한다면 이 인자에 0을 전달한다.*/
	//D3DXHANDLE h0 = ConstTable->GetComstantByName(0/*상수의 유효 범위 원하는 변수가 존재하는  */,"변수명")
	//ID3DXConstantTable::setXXX("상수 테이블과 연결된 장치의 포인터", "지정하려는 변수를 참조하는 핸들", "값");
	//배열을 지정할 때에는 마지막에 배열의 크기 속성이 추가된다.

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
	//해당 함수로 셰이더를 읽어오고
	HRESULT r = D3DXCompileShaderFromFile(
		"TestPixel.txt"
		, NULL
		, NULL
		, "PS_Main"/*진입점 함수*/
		, "vs_2_0"/*버전*/
		, dwFlags//컴파일 플래그
		, &buff//컴파일된 셰이더 코드를 가지는 ID3DXBUFFER포인터
		, &err//에러메시지 가지는 ID3DXBUFFER포인터
		, &pTbv//셰이더의 상수 테이블 데이터
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
	//해당 함수로 셰이더를 읽어오고
	D3DXCompileShaderFromFile(
		"../Resources/shader/Test.hlsli"
		, NULL
		, NULL
		,/*진입점 함수*/"VS_Main"
		,/*버전*/"vs_5_0"
		, dwFlags//컴파일 플래그
		, &buff//컴파일된 셰이더 코드를 가지는 ID3DXBUFFER포인터
		, &err//에러메시지 가지는 ID3DXBUFFER포인터
		, &pTbv//셰이더의 상수 테이블 데이터
	);
	//D3DXCompileShaderFromFile
	//d3dxCompile
	LPDIRECT3DVERTEXSHADER9 p_shader;
	g_pd3dDevice->CreateVertexShader((DWORD*)buff->GetBufferPointer(), &p_shader);

	//D3DVERTEXELEMENT9 vertecelement=
	//{
	//	/*버텍스 요소가 연결될 스트림을 지정한다.*/,
	//	/*버텍스 구조체 내의 버텍스 요소로의 시작을 가리키는 바이트 단위의 오프셋, 예를 들어 
	//	다음과 같은 버텍스 구조체가 있다면 (vector3 pos, vector3 normal) pos는 0 normal은 12 가 된다.*/,
	//	/*데이터 형을 지정한다. D3DDECLTYPE 열거형의 멤버 중 어떤 것이나 될 수 있다.*/
	//	/*부동 소수점 스칼라*/
	//	/*2D 부동 소수점 벡터*/
	//	/*3D 부동 소수점 벡터*/
	//	/*4D 부동 소수점 벡터*/
	//	/*칼라값*/,
	//	/*세분화(테셀레이션) 방식을 지정한다.*/,
	//	/*버텍스 요소의 계획된 용도를 지정한다. (위치벡터, 적스쳐좌표 등등)*/,
	//	/*버텍스 포인트의 크기를 지정, */,
	//	/*동일한 용도 내에서 다수의 버텍스 요소를 식별하는데 이용*/
	//}

	//하나의 위치벡터와 3개의 법선벡터
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
	g_pd3dDevice->SetVertexDeclaration(_decl); // SetFVF 대신 사용 set fvf도 내부적으로 Decl로 변형된다.

	/*버텍스 셰이더 입력 구조체의 데이터 멤버를 연결하는 앱을 정의하는 방법이 된다*/
	//실제 셰이더 코드에서의 입력 구조체
	/*struct VS_INPUT
	{
		vector position : POSITION;
		vector normal : NORMAL0;
		vector faceNormal : NORMAL1;
		vector faceNormal : NORMAL2;
	};
	요소  0은 decl이고 용도 POSITION과 용도 인덱스 0으로 식별되며 position으로 매핑된다.
	요소 1은 decl이고 용도 NORMAL과 용도 인덱스 0으로 식별되며 normal로 매핑된다.
	요소 2는 decl이고 용도 NORMAL과 용도 인덱스 1로 식별되며 faceNormal1로 매핑된다.....*/

	/*버텍스 입력 용도들
	POSITION -> 위치
	BLENDWEIGHTS -> 블렌드 가중치
	BLENDINDICES -> 블렌드 인덱스
	NORMAL -> 법선 벡터
	PSIZE -> 버텍스 포인트 크기
	DIFFUSE -> 난반사 컬러
	SPECULAR -> 정반사 컬러
	TEXCOORD -> 텍스쳐 좌표
	TANGENT -> 접선 벡터
	BINORMAL -> 이중법선 벡터
	TESSFACTOR -> 세분화 인수*/

	/*버텍스 출력 용도들
	POSITION -> 위치
	PSIZE -> 버텍스 포인트 크기
	FOG -> 안개 블렌드 값
	COLOR -> 버텍스 컬러, 다수의 버텍스 컬러를 출력 할 수 있다는 데 주의할것 이 컬러는 블렌드되어 최종 컬러를 만들어 낸다.
	TEXCOORD -> 버텍스 텍스쳐 좌표, 다수의 텍스쳐 좌표를 출력할 수 있다는 데 주의*/

	/*1. 버텍스 셰이더를 작성하고 컴파일 한다.
	  2. 컴파일된 셰이더 코드를 기반으로 버텍스 셰이더를 나타내는 IDirect3DVertexShader9 인터페이스를 만든가.
	  3. IDirect3DDevice9::SetVertexShader 메서드를 이용해 버텍스 셰이더를 활성화 한다.*/

	  //이용이 끝난 뒤에는 Release를 이용해 셰이더를 제거해야 한다.
	  //d3d::Release<IDirec>


}