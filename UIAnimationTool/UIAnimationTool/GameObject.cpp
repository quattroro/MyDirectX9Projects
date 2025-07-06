#include "stdafx.h"
#include "GameObject.h"
#include "Engine.h"
#include "Transform.h"
#include "UIBase.h"
#include "UIManager.h"
#include "UIScreen.h"
#include "FileManager.h"
#include "InputManager.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_neo_internal.h"
#include "imgui/imgui_neo_sequencer.h"


#include "UIAnimationTool.h"

int GameObject::_nextID = 0;

GameObject::~GameObject()
{
	delete m_transform;
	m_pVB2D->Release();
}

GameObject::GameObject()
{
	m_Selected = 0;
	m_InstanceID = _nextID++;
	m_transform = new Transform();
	tex = NULL;
	this->name = "";
	this->filename = "";
	//this->filename = "Resources/p_cs_item_name_01_00.dds";

	MakeVertexBuffer2D();

	//tex = gEngine->CreateTextureFromDDS("Resources/p_cs_item_name_01_00.dds", &width, &height);
}

GameObject::GameObject(const char* name, const char* filename)
{
	m_Selected = 0;
	m_InstanceID = _nextID++;
	m_transform = new Transform();
	tex = NULL;
	this->name = name;
	this->filename = "";
	width = 0;
	height = 0;

	MakeVertexBuffer2D();
}

GameObject::GameObject(const GameObject* object)
{
	m_Selected = 0;
	m_InstanceID = _nextID++;
	m_transform = new Transform();
	memcpy(m_transform, object->m_transform, sizeof(Transform));
	name = CheckObjectName(object->name.c_str());
	filename = object->filename;
	filepath = object->filepath;
	MakeVertexBuffer2D();

	int tempwidth;
	int tempheight;
	tex = g_FileManager->LoadTexture(&width, &height, object->filepath.c_str());
	m_transform->m_size = D3DXVECTOR2(width, height);
}

bool GameObject::operator== (const GameObject& other)
{
	return m_InstanceID == other.m_InstanceID;
}

bool GameObject::operator== (const int InstanceID)
{
	return m_InstanceID == InstanceID;
}

void GameObject::SetViewport()
{

}

void GameObject::GetViewport()
{

}

RECT GameObject::GetRect()
{
	D3DXVECTOR2 pos = g_UIManager->GetUI(UI_TYPE::SCREEN)->GetUIPos();
	D3DXVECTOR2 size = g_UIManager->GetUI(UI_TYPE::SCREEN)->GetUISize();

	RECT rt;
	gEngine->GetCurClientRect(rt);

	/*m_Rect.left = pos.x - rt.left + m_transform->m_position.x;
	m_Rect.top = pos.y - rt.top + m_transform->m_position.y;
	m_Rect.right = m_Rect.left + width;
	m_Rect.bottom = m_Rect.top + height;*/

	/*D3DXVECTOR3 pos1 = D3DXVECTOR3(-(int)width/2.0, -(int)height/2.0 ,0);
	D3DXVECTOR3 pos2 = D3DXVECTOR3((int)width/2.0, (int)height/2.0,0);*/

	D3DXVECTOR3 pos1 = D3DXVECTOR3(-0.5, -0.5, 0);
	D3DXVECTOR3 pos2 = D3DXVECTOR3(0.5, 0.5, 0);

	D3DXVec3TransformCoord(&pos1, &pos1, &m_matWorld);
	D3DXVec3TransformCoord(&pos2, &pos2, &m_matWorld);

	m_Rect.left = pos1.x;
	m_Rect.top = pos1.y;
	m_Rect.right = pos2.x;
	m_Rect.bottom = pos2.y;

	return m_Rect;
}

D3DXVECTOR3* GameObject::GetCorner()
{
	m_Rect2[0] = D3DXVECTOR3(-0.5, -0.5, 0.0);
	m_Rect2[1] = D3DXVECTOR3(0.5, -0.5, 0.0);
	m_Rect2[2] = D3DXVECTOR3(0.5, 0.5, 0.0);
	m_Rect2[3] = D3DXVECTOR3(-0.5, 0.5, 0.0);

	for (int i = 0; i < 4; i++)
		D3DXVec3TransformCoord(&m_Rect2[i], &m_Rect2[i], &m_matWorld);

	return m_Rect2;
}

//RECT GameObject::GetRect2()
//{
//	D3DXVECTOR3* corners = GetCorner();
//	m_Rect.left = corners[0].x;
//	m_Rect.top = corners[0].y;
//
//
//	/*m_Rect2[0] = D3DXVECTOR3(-0.5, -0.5, 0.0);
//	m_Rect2[1] = D3DXVECTOR3(0.5, -0.5, 0.0);
//	m_Rect2[2] = D3DXVECTOR3(0.5, 0.5, 0.0);
//	m_Rect2[3] = D3DXVECTOR3(-0.5, 0.5, 0.0);
//
//	for (int i = 0; i < 4; i++)
//		D3DXVec3TransformCoord(&m_Rect2[i], &m_Rect2[i], &m_matWorld);*/
//
//	return m_Rect;
//}

bool GameObject::Intersects(D3DXVECTOR2 pt)
{
	//D3DVIEWPORT9  tmpViewport = ((UIScreen*)g_UIManager->GetUI(UI_TYPE::SCREEN))->GetViewport();

	D3DXMATRIX invWorld;
	D3DXMatrixInverse(&invWorld, NULL, &m_matWorld);

	D3DXVECTOR4 mposw;
	/*D3DXVECTOR2 tempMousePos = mousePos - D3DXVECTOR2(tmpViewport.X, tmpViewport.Y) - D3DXVECTOR2(tmpViewport.Width / 2, tmpViewport.Height / 2);
	mousePosWorld = tempMousePos;*/

	//마우스 포인터를 물체의 로컬 좌표로 보낸다
	D3DXVec2Transform(&mposw, &pt, &invWorld);
	/*mousePosWorld.x = mposw.x;
	mousePosWorld.y = mposw.y;*/
	POINT point = { mposw.x, mposw.y };

	if (mposw.x >= -0.5 && mposw.x <= 0.5
		&& mposw.y >= -0.5 && mposw.y <= 0.5)
	{
		return true;
	}

	return false;
}

void GameObject::ReleaseTexture()
{
	tex->Release();
	tex = NULL;
}

void GameObject::Render()
{
	if (filename.length() <= 0)
		return;


	D3DVIEWPORT9  oldViewport;
	gEngine->GetDevice()->GetViewport(&oldViewport);

	D3DXVECTOR2 pos = g_UIManager->GetUI(UI_TYPE::SCREEN)->GetUIPos();
	D3DXVECTOR2 size = g_UIManager->GetUI(UI_TYPE::SCREEN)->GetUISize();


	D3DXVECTOR2 objPos = m_transform->m_position;
	D3DXVECTOR2 objSize = m_transform->m_size;
	D3DXVECTOR2 objScale = m_transform->m_scale;
	float objRotation = m_transform->m_rotation;
	float opacity = m_transform->m_opacity;

	D3DVIEWPORT9  tmpViewport = ((UIScreen*)g_UIManager->GetUI(UI_TYPE::SCREEN))->GetViewport();

	gEngine->GetDevice()->SetViewport(&tmpViewport);
	size = D3DXVECTOR2(tmpViewport.Width, tmpViewport.Height);

	D3DXMATRIX rotMat;

	D3DXMatrixIdentity(&matWorld);
	D3DXMatrixIdentity(&matView);
	D3DXMatrixIdentity(&matProj);
	D3DXMatrixIdentity(&rotMat);

	matWorld._11 += width + objSize.x;
	matWorld._22 += height + objSize.y;

	matWorld._11 *= objScale.x;
	matWorld._22 *= objScale.y;

	D3DXMatrixRotationZ(&rotMat, objRotation * 3.141592 / 180.0);
	matWorld = matWorld * rotMat;

	matWorld._41 += objPos.x;
	matWorld._42 += objPos.y;

	/*matWorld._41 += width / 2.0;
	matWorld._42 += height / 2.0;*/

	m_matWorld = matWorld;

	int tempWidth = g_UIManager->GetCanvasSize()[0];
	int tempHeight = g_UIManager->GetCanvasSize()[1];
	float ratio = (float)tempWidth / (float)tempHeight;

	/*size.y -= g_InputManager->MouseWheel();
	size.x = size.y * ratio;*/
	size.x = tempWidth;
	size.y = tempHeight;

	D3DXMatrixOrthoOffCenterLH(&matProj, -size.x / 2, size.x / 2, size.y / 2, -size.y / 2, 0, 10);
	//D3DXMatrixOrthoOffCenterLH(&matProj, 0, wheight * fRatio, wheight, 0, 0, 1);

	/*D3DXVECTOR3 vEyePt(D3DXVECTOR3(tempPos.x, tempPos.y, tempPos.z));
	D3DXVECTOR3 vLookatPt(D3DXVECTOR3(tempPos.x, tempPos.y, 0));
	D3DXVECTOR3 vUpVec(D3DXVECTOR3(0, 1, 0));
	D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);*/
	/*matView._41 = tempPos.x;
	matView._42 = tempPos.y;
	matView._43 = tempPos.z;*/


	gEngine->GetDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	gEngine->GetDevice()->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	//Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	gEngine->GetDevice()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	gEngine->GetDevice()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);


	gEngine->GetDevice()->SetTransform(D3DTS_WORLD, &matWorld);
	gEngine->GetDevice()->SetTransform(D3DTS_VIEW, &matView);
	gEngine->GetDevice()->SetTransform(D3DTS_PROJECTION, &matProj);

	gEngine->GetDevice()->SetTexture(0, tex);
	gEngine->GetDevice()->SetFVF(FVF_T);
	gEngine->GetDevice()->SetStreamSource(0, m_pVB2D, 0, sizeof(VERTEX_T));

	//D3DXMATRIXA16 matWVP;
	m_matWVP = matWorld * matView * matProj;

	m_matVP = matView * matProj;
	D3DXHANDLE	dxHandle;
	D3DXVECTOR4 color = m_transform->m_color;
	color.w = opacity;


	gEngine->GetShader()->SetTechnique("UIDraw");
	dxHandle = gEngine->GetShader()->GetParameterByName(0, "_matWorldViewProjection");
	gEngine->GetShader()->SetMatrix(dxHandle, &m_matWVP);
	dxHandle = gEngine->GetShader()->GetParameterByName(0, "_Bright");
	gEngine->GetShader()->SetFloat(dxHandle, 1.0f);
	dxHandle = gEngine->GetShader()->GetParameterByName(0, "_Color");
	gEngine->GetShader()->SetVector(dxHandle, &color);
	dxHandle = gEngine->GetShader()->GetParameterByName(0, "_matWorld");
	gEngine->GetShader()->SetMatrix(dxHandle, &matWorld);
	dxHandle = gEngine->GetShader()->GetParameterByName(0, "_matView");
	gEngine->GetShader()->SetMatrix(dxHandle, &matView);
	dxHandle = gEngine->GetShader()->GetParameterByName(0, "_matProj");
	gEngine->GetShader()->SetMatrix(dxHandle, &matProj);

	UINT passCount;
	gEngine->GetShader()->Begin(&passCount, 0);
	for (int pass = 0; pass < passCount; pass++)
	{
		gEngine->GetShader()->BeginPass(pass);
		gEngine->GetDevice()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
		gEngine->GetShader()->EndPass();
	}
	gEngine->GetShader()->End();


	if (m_Selected)
	{
		// directx 라인 그리기 함수가 viewport가 적용이 안됨...
		D3DXVECTOR3* rtx = GetCorner();
		D3DXMATRIX matViewport;
		D3DXMATRIX result;
		D3DXMatrixIdentity(&matViewport);
		D3DXMatrixIdentity(&result);
		//D3DVIEWPORT9  tmpViewport = ((UIScreen*)g_UIManager->GetUI(UI_TYPE::SCREEN))->GetViewport();
		matViewport._11 = tmpViewport.Width / 2.0;
		matViewport._41 = tmpViewport.X + tmpViewport.Width / 2.0;
		matViewport._22 = -tmpViewport.Height / 2.0;
		matViewport._42 = tmpViewport.Y + tmpViewport.Height / 2.0;
		matViewport._33 = tmpViewport.MaxZ - tmpViewport.MinZ;
		matViewport._43 = tmpViewport.MinZ;

		result = m_matVP * matViewport;
		result = m_matVP;

		/*size = D3DXVECTOR2(tmpViewport.Width, tmpViewport.Height);
		size.y -= g_InputManager->MouseWheel();
		size.x = size.y * ratio;
		D3DXMatrixOrthoOffCenterLH(&tempProj, -size.x / 2, size.x / 2, size.y / 2, -size.y / 2, 0, 10);*/

		/*D3DXVECTOR3 vertex1[2];
		vertex1[0] = rtx[0];
		vertex1[1] = rtx[1];

		D3DXVECTOR3 vertex2[2];
		vertex2[0] = rtx[1];
		vertex2[1] = rtx[2];

		D3DXVECTOR3 vertex3[2];
		vertex3[0] = rtx[2];
		vertex3[1] = rtx[3];

		D3DXVECTOR3 vertex4[2];
		vertex4[0] = rtx[0];
		vertex4[1] = rtx[3];*/

		/*mpViewport.X += tempPos.x;
		tmpViewport.Y += tempPos.y;*/

		D3DXVECTOR3 vertex1[2];
		vertex1[0] = rtx[0] - D3DXVECTOR3(tmpViewport.X, tmpViewport.Y, 0);
		vertex1[1] = rtx[1] - D3DXVECTOR3(tmpViewport.X, tmpViewport.Y, 0);

		D3DXVECTOR3 vertex2[2];
		vertex2[0] = rtx[1] - D3DXVECTOR3(tmpViewport.X, tmpViewport.Y, 0);
		vertex2[1] = rtx[2] - D3DXVECTOR3(tmpViewport.X, tmpViewport.Y, 0);

		D3DXVECTOR3 vertex3[2];
		vertex3[0] = rtx[2] - D3DXVECTOR3(tmpViewport.X, tmpViewport.Y, 0);
		vertex3[1] = rtx[3] - D3DXVECTOR3(tmpViewport.X, tmpViewport.Y, 0);

		D3DXVECTOR3 vertex4[2];
		vertex4[0] = rtx[0] - D3DXVECTOR3(tmpViewport.X, tmpViewport.Y, 0);
		vertex4[1] = rtx[3] - D3DXVECTOR3(tmpViewport.X, tmpViewport.Y, 0);

		D3DXCOLOR linecolor = D3DXCOLOR(1, 1, 1, 1);
		D3DXCreateLine(gEngine->GetDevice(), &m_Line);

		m_Line->SetWidth(1.0);
		m_Line->Begin();
		m_Line->DrawTransform(vertex1, 2, /*&m_matVP*/&result, linecolor);
		m_Line->DrawTransform(vertex2, 2, /*&m_matVP*/&result, linecolor);
		m_Line->DrawTransform(vertex3, 2, /*&m_matVP*/&result, linecolor);
		m_Line->DrawTransform(vertex4, 2, /*&m_matVP*/&result, linecolor);
		m_Line->End();
		m_Line->Release();
	}

	gEngine->GetDevice()->SetViewport(&oldViewport);
}

void GameObject::SetTexture(LPDIRECT3DTEXTURE9 tex, int width, int height, const char* filename, const char* filepath)
{
	this->tex = tex;
	this->width = width;
	this->height = height;
	this->filename = filename;
	this->filepath = filepath;
	this->m_transform->m_size = D3DXVECTOR2(width, height);
}


void GameObject::MakeVertexBuffer2D()
{
	gEngine->GetDevice()->CreateVertexBuffer(4 * sizeof(VERTEX_T), D3DUSAGE_WRITEONLY, FVF_T, D3DPOOL_MANAGED, &m_pVB2D, NULL);
	VERTEX_T* pVertices = NULL;

	m_pVB2D->Lock(0, 4 * sizeof(VERTEX_T), (void**)&pVertices, 0);
	if (pVertices == NULL)
	{
		m_pVB2D->Unlock();
		return;
	}

	pVertices[0].x = -0.5f;
	pVertices[0].y = -0.5f;
	pVertices[0].z = 0.0f;

	pVertices[1].x = 0.5f;
	pVertices[1].y = -0.5f;
	pVertices[1].z = 0.0f;

	pVertices[2].x = -0.5f;
	pVertices[2].y = 0.5f;
	pVertices[2].z = 0.0f;

	pVertices[3].x = 0.5f;
	pVertices[3].y = 0.5f;
	pVertices[3].z = 0.0f;

	pVertices[0].tu = 0.0f;
	pVertices[0].tv = 0.0f;

	pVertices[1].tu = 1.0f;
	pVertices[1].tv = 0.0f;

	pVertices[2].tu = 0.0f;
	pVertices[2].tv = 1.0f;

	pVertices[3].tu = 1.0f;
	pVertices[3].tv = 1.0f;

	m_pVB2D->Unlock();
}
