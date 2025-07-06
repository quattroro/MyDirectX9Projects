#include "stdafx.h"
#include "UIBase.h"
#include "UIScreen.h"
#include "UITransform.h"
#include "UIManager.h"
#include "UIHierarkhy.h"
#include "GameObject.h"
#include "Engine.h"
#include "UIAnimationTool.h"
#include "InputManager.h"
#include "UITransform.h"
#include "ObjectManager.h"
#include "FileManager.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_neo_internal.h"
#include "imgui/imgui_neo_sequencer.h"

UIScreen::UIScreen(UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix) :UIBase(manager, name, pos, size, posFix)
{
	m_isSelect = false;
	m_type = UI_TYPE::SCREEN;
	canvasObject = new GameObject("Canvas");
	canvasObject->GetTransform()->m_scale = D3DXVECTOR2(500, 500);
	DWORD width;
	DWORD height;
	char filename[125] = { 0 };
	char filepath[1024] = { 0 };
	LPDIRECT3DTEXTURE9 tex = g_FileManager->LoadTexture(&width, &height, "Resources\\Rect_White.png");
	if (tex)
	{
		canvasObject->SetTexture(tex, width, height, "Rect_White.png", filepath);
	}
}

UIScreen::~UIScreen()
{

}

D3DVIEWPORT9 UIScreen::GetViewport()
{
	D3DXVECTOR2 pos = GetUIPos();
	D3DXVECTOR2 size = GetUISize();
	RECT rt;
	gEngine->GetCurClientRect(rt);

	int tempWidth = g_UIManager->GetCanvasSize()[0];
	int tempHeight = g_UIManager->GetCanvasSize()[1];
	float ratio = (float)tempWidth / (float)tempHeight;

	tempHeight += g_InputManager->MouseWheel();
	tempWidth = tempHeight * ratio;

	D3DXVECTOR2 center = D3DXVECTOR2(pos.x - rt.left - 12 + size.x / 2, pos.y - rt.top - 45 + size.y / 2);
	D3DVIEWPORT9  tmpViewport;
	tmpViewport.X = (center.x - tempWidth / 2);
	tmpViewport.Y = (center.y - tempHeight / 2);
	tmpViewport.Width = tempWidth;
	tmpViewport.Height = tempHeight;

	if ((center.y - tempHeight / 2) < pos.y - rt.top - 45)
	{
		tmpViewport.Y = pos.y - rt.top - 45;
		tmpViewport.Height = size.y;
		tempHeight = size.y;

		tempWidth = tmpViewport.Height * ratio;
		tmpViewport.X = center.x - tempWidth / 2;
		tmpViewport.Width = tempWidth;
	}

	if ((center.x - tempWidth / 2) < pos.x - rt.left - 12)
	{
		tmpViewport.X = pos.x - rt.left - 12;
		tmpViewport.Width = size.x;
		tempWidth = size.x;

		tempHeight = tmpViewport.Width / ratio;
		tmpViewport.Y = center.y - tempHeight / 2;
		tmpViewport.Height = tempHeight;
	}
	tmpViewport.MinZ = 0.0;
	tmpViewport.MaxZ = 1.0;

	/*D3DVIEWPORT9  tmpViewport;
	tmpViewport.X = pos.x - rt.left - 12;
	tmpViewport.Y = pos.y - rt.top - 45;
	tmpViewport.Width = size.x;
	tmpViewport.Height = size.y;
	tmpViewport.MinZ = 0.0;
	tmpViewport.MaxZ = 1.0;*/

	return tmpViewport;
}

void UIScreen::Render()
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove;
	bool open = true;
	ImGui::Begin("Screen", &open, window_flags);
	UpdateWindowInfo();

	D3DVIEWPORT9  tmpViewport = GetViewport();
	D3DXVECTOR2 temppos = g_InputManager->GetMousePos() - D3DXVECTOR2(tmpViewport.X, tmpViewport.Y) - D3DXVECTOR2(tmpViewport.Width / 2, tmpViewport.Height / 2);
	ImGui::Text("mouse = (%f, %f)", temppos.x, temppos.y);
	ImGui::Text("temppos = (%f, %f, %f)", tempPos.x, tempPos.y, tempPos.z);
	ImGui::Text("wheel = (%d)", g_InputManager->MouseWheel());

	/*for (int i = 0; i < g_ObjectManager->m_HistoryList.size(); i++)
	{
		ImGui::Text("Stack[%d] = (%d)", i, g_ObjectManager->m_HistoryList[i]);
	}*/

	//ImVec2 vMin = ImVec2(99999, 99999);
	//ImVec2 vMax = ImVec2(0,0);

	//ImVec2 p1;
	//ImVec2 p2;
	//ImVec2 p3;
	//ImVec2 p4;

	//D3DXVECTOR3* temprect;

	//RECT rt;
	//RECT objrt = { 99999,99999,0,0 };
	////POINT pt = { mousePos.x, mousePos.y };

	//gEngine->GetCurClientRect(rt);

	//for (pair<int, GameObject*> m : g_UIManager->m_SelectObjectMap)
	//{
	//	vMin.x = min(vMin.x, m.second->GetRect().left + /*rt.left*/m_Pos.x + m_Size.x / 2);
	//	vMin.y = min(vMin.y, m.second->GetRect().top + /*rt.top*/m_Pos.y + m_Size.y / 2);
	//	
	//	vMax.x = max(vMax.x, m.second->GetRect().right + /*rt.left*/m_Pos.x + m_Size.x / 2);
	//	vMax.y = max(vMax.y, m.second->GetRect().bottom + /*rt.top*/m_Pos.y + m_Size.y / 2);

	//	temprect = m.second->GetCorner();

	//	/*objrt = m.second->GetRect();

	//	objrt.left = min(objrt.left, m.second->GetRect().left - 12);
	//	objrt.top = min(objrt.top, m.second->GetRect().top - 45);
	//	objrt.right = max(objrt.right, m.second->GetRect().right - 12);
	//	objrt.bottom = max(objrt.bottom, m.second->GetRect().bottom - 45);*/
	//}




	ImGui::End();
}

void UIScreen::Update()
{
	if (IsFocus())
	{
		if (g_InputManager->MouseLDown())
		{
			D3DVIEWPORT9  tmpViewport = GetViewport();
			D3DXVECTOR2 downpos = g_InputManager->GetMouseDownPos() - D3DXVECTOR2(tmpViewport.X, tmpViewport.Y) - D3DXVECTOR2(tmpViewport.Width / 2, tmpViewport.Height / 2);
			clickpos = g_InputManager->GetMouseDownPos();

			for (GameObject* o : g_UIManager->m_ObjectList)
			{
				if (o->Intersects(downpos))
				{
					g_UIManager->SelectObject(o, true);
					m_isSelect = true;

				}
			}
		}

		if (m_isSelect && g_InputManager->MouseLPress())
		{
			D3DXVECTOR2 draggPos = g_InputManager->GetMousePos() - clickpos;
			if (D3DXVec2LengthSq(&draggPos) > 0)
			{
				Transform transform = (*g_UIManager->GetSelectedObject().begin()).second->GetTransform();
				transform.m_position += draggPos;

				((UITransform*)g_UIManager->GetUI(UI_TYPE::TRANSFORM))->SetTransform((*g_UIManager->GetSelectedObject().begin()).second, transform);

				clickpos = g_InputManager->GetMousePos();
			}
		}

		if (g_InputManager->MouseLUp())
			m_isSelect = false;
	}



}

void UIScreen::Input()
{

}