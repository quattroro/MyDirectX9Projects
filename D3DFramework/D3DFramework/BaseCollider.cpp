#include "BaseCollider.h"
//#include "SphereCollider.h"


struct XMPosVertex
{
	DirectX::XMFLOAT3 p;
};

BaseCollider::BaseCollider()
{
	m_CollType = ColliderType::Sphere;
	//LPD3DXLINE Line;
	D3DXCreateLine(g_pd3dDevice, &Line);
	Line->SetWidth(2.0);
	Line->SetAntialias(TRUE);
	g_pd3dDevice->CreateVertexBuffer(100 * sizeof(XMPosVertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &pVB, NULL);
	static const D3DVERTEXELEMENT9 elements[2] =
	{
		{ 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
		D3DDECL_END()
	};

	g_pd3dDevice->CreateVertexDeclaration(elements, &pVertexDecl);

	m_CollChannel = Collision_Channel::GENERAL;
	ZeroMemory(m_CollFilter, sizeof(int) * (int)Collision_Channel::Collision_Channel_Max);
}

BaseCollider::BaseCollider(ColliderType type) : BaseCollider()
{
	m_CollType = type;
}

BaseCollider::~BaseCollider()
{
	Line->Release();
	pVB->Release();
	//pVBRelease(pVB);
}

void BaseCollider::RenderLine(D3DXVECTOR3* pos1, D3DXVECTOR3* pos2, D3DXCOLOR col)
{
	D3DXMATRIX matWVP;
	D3DXMatrixIdentity(&matWVP);
	D3DXMatrixMultiply(&matWVP, /*&matWorld*/&matWVP, &matView);
	D3DXMatrixMultiply(&matWVP, &matWVP, &matProj);

	D3DXVECTOR3 vertex2[2];
	vertex2[0].x = pos1->x;
	vertex2[0].y = pos1->y;
	vertex2[0].z = pos1->z;

	vertex2[1].x = pos2->x;
	vertex2[1].y = pos2->y;
	vertex2[1].z = pos2->z;

	Line->DrawTransform(vertex2, 2, &matWVP, col);

}

void BaseCollider::TransForm(D3DXMATRIX& transMat)
{
}



void BaseCollider::DrawRing(IDirect3DDevice9* pd3dDevice, const DirectX::XMFLOAT3& Origin, const DirectX::XMFLOAT3& MajorAxis, const DirectX::XMFLOAT3& MinorAxis, D3DCOLOR Color)
{
	static const DWORD dwRingSegments = 32;

	DirectX::XMFLOAT3 verts[dwRingSegments + 1];

	DirectX::XMVECTOR vOrigin = XMLoadFloat3(&Origin);
	DirectX::XMVECTOR vMajor = XMLoadFloat3(&MajorAxis);
	DirectX::XMVECTOR vMinor = XMLoadFloat3(&MinorAxis);

	FLOAT fAngleDelta = DirectX::XM_2PI / (float)dwRingSegments;
	// Instead of calling cos/sin for each segment we calculate
	// the sign of the angle delta and then incrementally calculate sin
	// and cosine from then on.
	DirectX::XMVECTOR cosDelta = DirectX::XMVectorReplicate(cosf(fAngleDelta));
	DirectX::XMVECTOR sinDelta = DirectX::XMVectorReplicate(sinf(fAngleDelta));
	DirectX::XMVECTOR incrementalSin = DirectX::XMVectorZero();
	static const DirectX::XMVECTOR initialCos =
	{
		1.0f, 1.0f, 1.0f, 1.0f
	};
	DirectX::XMVECTOR incrementalCos = initialCos;
	for (DWORD i = 0; i < dwRingSegments; i++)
	{
		DirectX::XMVECTOR Pos;
		Pos = DirectX::XMVectorMultiplyAdd(vMajor, incrementalCos, vOrigin);
		Pos = DirectX::XMVectorMultiplyAdd(vMinor, incrementalSin, Pos);
		DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&verts[i], Pos);
		// Standard formula to rotate a vector.
		DirectX::XMVECTOR newCos = DirectX::XMVectorSubtract(DirectX::XMVectorMultiply(incrementalCos, cosDelta), DirectX::XMVectorMultiply(incrementalSin, sinDelta));
		DirectX::XMVECTOR newSin = DirectX::XMVectorAdd(DirectX::XMVectorMultiply(incrementalCos, sinDelta), DirectX::XMVectorMultiply(incrementalSin, cosDelta));
		incrementalCos = newCos;
		incrementalSin = newSin;
	}
	verts[dwRingSegments] = verts[0];

	// Copy to vertex buffer
	//assert((dwRingSegments + 1) <= MAX_VERTS);

	DirectX::XMFLOAT3* pVerts = NULL;
	HRESULT hr;
	pVB->Lock(0, 0, (void**)&pVerts, D3DLOCK_DISCARD);
	memcpy(pVerts, verts, sizeof(verts));
	pVB->Unlock();

	CShader* m_shader = g_ShaderManager->GetShader(UI_SHADER);

	// Draw ring
	//D3DXCOLOR clr = Color;
//g_pEffect9->SetFloatArray(g_Color, clr, 4);
//g_pEffect9->CommitChanges();
//pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
/*pd3dDevice->SetTransform(D3DTS_VIEW, &matView);
pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);*/

/*pd3dDevice->SetStreamSource(0, pVB, 0, sizeof(VERTEX));
DLT_SetFVF(FVF_);*/

//Device->SetTFactor(Color);
//Device->SetTFactor(D3DCOLOR_XRGB(32, 16, 5));
////	Device->SetTFactor(D3DCOLOR_XRGB(255, 255, 255));
//Device->SetOp(((TCURRENT | TFACTOR | TADD) << TSTG1) | (TTEXTURE | TDIFFUSE), TTEXTURE);


	(pd3dDevice->SetStreamSource(0, pVB, 0, sizeof(XMPosVertex)));
	(pd3dDevice->SetVertexDeclaration(pVertexDecl));

	m_shader->SetShaderTechnique("RenderScene");
	D3DXMATRIX matWVP = /*matWorld **/ matView * matProj;
	m_shader->SetShaderMatrixVariable("_matWorldViewProjection", matWVP);
	float cr = ((Color >> 16) & 0xFF) / 255.f;
	float cg = ((Color >> 8) & 0xFF) / 255.f;
	float cb = ((Color >> 0) & 0xFF) / 255.f;
	float ca = ((Color >> 24) & 0xFF) / 255.f;
	m_shader->SetShaderVector4Variable("_Color", D3DXVECTOR4(cr, cg, cb, ca));

	UINT passCount;
	m_shader->ShaderGetPass(&passCount);
	for (int pass = 0; pass < passCount; pass++)
	{
		m_shader->ShaderBeginPass(pass);

		pd3dDevice->DrawPrimitive(D3DPT_LINESTRIP, 0, dwRingSegments);

		m_shader->ShaderEndPass();

	}
	m_shader->ShaderEnd();

}

void BaseCollider::DrawSphere(IDirect3DDevice9* pd3dDevice, BoundingSphere& sphere, D3DCOLOR Color)
{
	const DirectX::XMFLOAT3 Origin = (DirectX::XMFLOAT3)sphere.Center;
	const float fRadius = sphere.Radius;

	DrawRing(pd3dDevice, Origin, DirectX::XMFLOAT3(fRadius, 0, 0), DirectX::XMFLOAT3(0, 0, fRadius), Color);
	DrawRing(pd3dDevice, Origin, DirectX::XMFLOAT3(fRadius, 0, 0), DirectX::XMFLOAT3(0, fRadius, 0), Color);
	DrawRing(pd3dDevice, Origin, DirectX::XMFLOAT3(0, fRadius, 0), DirectX::XMFLOAT3(0, 0, fRadius), Color);
}


