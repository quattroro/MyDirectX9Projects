#include "stdafx.h"
#include "UIBase.h"
#include "UIManager.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_neo_internal.h"
#include "imgui/imgui_neo_sequencer.h"

UIBase::UIBase(UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix) :m_UIManager(manager), m_Pos(pos), m_Size(size), m_PosFix(posFix)
{

}

UIBase::~UIBase()
{

}


void UIBase::UpdateWindowInfo()
{
	if (ImGui::IsWindowFocused())
		m_isFocus = true;
	else
		m_isFocus = false;


	ImVec2 vMin = ImGui::GetWindowContentRegionMin();
	ImVec2 vMax = ImGui::GetWindowContentRegionMax();

	vMin.x += ImGui::GetWindowPos().x;
	vMin.y += ImGui::GetWindowPos().y;
	vMax.x += ImGui::GetWindowPos().x;
	vMax.y += ImGui::GetWindowPos().y;

	//ImGui::GetForegroundDrawList()->AddRect(vMin, vMax, IM_COL32(255, 255, 0, 255));

	m_Size.x = ImGui::GetWindowSize().x;
	m_Size.y = ImGui::GetWindowSize().y;
	m_Size.x = vMax.x - vMin.x;
	m_Size.y = vMax.y - vMin.y;
	m_Pos.x = vMin.x;
	m_Pos.y = vMin.y;
}

void UIBase::UIBegin()
{

}

void UIBase::UIEnd()
{

}

D3DXVECTOR2 UIBase::GetUISize()
{
	return m_Size;
}

D3DXVECTOR2 UIBase::GetUIPos()
{
	return m_Pos;
}

D3DXVECTOR2 UIBase::GetUIPosWorld()
{
	D3DXVECTOR2 pos = m_Pos;
	pos.x -= 10;
	pos.y -= 45;
	return pos;
}