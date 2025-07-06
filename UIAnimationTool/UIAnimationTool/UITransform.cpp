#include "stdafx.h"
#include "UIBase.h"
#include "UITransform.h"
#include "UIManager.h"
#include "UIHierarkhy.h"
#include "GameObject.h"
#include "Transform.h"
#include "UIAnimationTool.h"
#include "UISequencer.h"
#include "UIBezierController.h"
#include "FileManager.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_neo_internal.h"
#include "imgui/imgui_neo_sequencer.h"

UITransform::UITransform(UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix) :UIBase(manager, name, pos, size, posFix)
{
	m_Transform = new Transform();
	m_ChangeName = false;
	m_type = UI_TYPE::TRANSFORM;
	LockFlag = 0;
}

UITransform::~UITransform()
{

}

void UITransform::Render()
{
	map<int, GameObject*> m_SelectObject = m_UIManager->GetSelectedObject();

	GameObject* FirstSelectedObj = nullptr;
	Transform oldTransform;
	if (m_SelectObject.size())
	{
		FirstSelectedObj = (*m_SelectObject.begin()).second;
		memcpy(m_Transform, (*m_SelectObject.begin()).second->GetTransform(), sizeof(Transform));
		memcpy(&oldTransform, (*m_SelectObject.begin()).second->GetTransform(), sizeof(Transform));
	}

	char objectName[125] = { 0 };
	char resourceName[125] = { 0 };

	if (m_SelectObject.size() > 1)
	{
		sprintf(objectName, "MultiObject");
		sprintf(resourceName, "MultiObject");
	}
	else if (m_SelectObject.size() <= 0)
	{
		sprintf(objectName, "None");
		sprintf(resourceName, "None");
	}
	else
	{
		for (pair<int, GameObject*> m : m_SelectObject)
		{
			sprintf(objectName, "%s", m.second->GetName().c_str());
			sprintf(resourceName, "%s", m.second->GetFileName().c_str());
		}
	}

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove;
	bool open = true;
	bool change = false;
	ImGui::Begin("Object", &open, window_flags);
	UpdateWindowInfo();

	ImGui::Text("%f", g_CurFrame);
	ImGui::SeparatorText("ObjectName");

	if (ImGui::InputText("Name", objectName, 125))
	{
		FirstSelectedObj->SetName(objectName);
	}

	if (FirstSelectedObj && ((UISequencer*)m_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetGroup(FirstSelectedObj) != nullptr)
	{
		ImGui::Text("Resource : %s", resourceName);
		ImGui::SameLine();
		if (ImGui::SmallButton("Texture") && m_SelectObject.size())
		{
			DWORD width;
			DWORD height;
			char filename[125] = { 0 };
			char filepath[1024] = { 0 };
			LPDIRECT3DTEXTURE9 tex = g_FileManager->LoadTexture(&width, &height, filename, filepath);
			if (tex)
			{
				(*m_SelectObject.begin()).second->SetTexture(tex, width, height, filename, filepath);
			}
		}
		ImGui::Text("Path : %s", FirstSelectedObj->GetFilePath().c_str());
		ImGui::SeparatorText("Transform");
		{
			change |= ImGui::DragFloat2("Position", m_Transform->m_position, 1.0);
			change |= ImGui::DragFloat2("size", m_Transform->m_size, 1.0);
			//ImGui::SameLine();
			//if (ImGui::CheckboxFlags("Lock", &LockFlag, TransformLock_Size))
			//{
			//	if ((LockFlag & TransformLock_Size))
			//	{
			//		SizeRatio.x = /*(float)FirstSelectedObj->GetTexHeight() + */(float)m_Transform->m_size.y / /*(float)FirstSelectedObj->GetTexWidth() + */(float)m_Transform->m_size.x;
			//		SizeRatio.y = /*(float)FirstSelectedObj->GetTexWidth() + */(float)m_Transform->m_size.x / /*(float)FirstSelectedObj->GetTexHeight() + */(float)m_Transform->m_size.y;
			//	}
			//}
			//if ((LockFlag & TransformLock_Size))
			//{
			//	float ratio;
			//	if (oldTransform.m_size.x != m_Transform->m_size.x)
			//	{
			//		/*ratio = (float)FirstSelectedObj->GetTexHeight() / (float)FirstSelectedObj->GetTexWidth();
			//		m_Transform->m_size.y = m_Transform->m_size.x * ratio;*/

			//		m_Transform->m_size.y = m_Transform->m_size.x * SizeRatio.x;
			//	}
			//	else if (oldTransform.m_size.y != m_Transform->m_size.y)
			//	{
			//		/*ratio = (float)FirstSelectedObj->GetTexWidth() / (float)FirstSelectedObj->GetTexHeight();
			//		m_Transform->m_size.x = m_Transform->m_size.y * ratio;*/

			//		m_Transform->m_size.x = m_Transform->m_size.y * SizeRatio.y;
			//	}
			//}


			change |= ImGui::DragFloat2("Scale", m_Transform->m_scale, 0.01);
			ImGui::SameLine();
			if (ImGui::CheckboxFlags("Lock##2", &LockFlag, TransformLock_Scale))
			{
				if ((LockFlag & TransformLock_Scale))
				{
					ScaleRatio.x = (float)m_Transform->m_scale.y / (float)m_Transform->m_scale.x;
					ScaleRatio.y = (float)m_Transform->m_scale.x / (float)m_Transform->m_scale.y;
				}
			}
			if ((LockFlag & TransformLock_Scale))
			{
				float ratio;
				if (oldTransform.m_scale.x != m_Transform->m_scale.x)
				{
					m_Transform->m_scale.y = m_Transform->m_scale.x * ScaleRatio.x;
				}
				else if (oldTransform.m_scale.y != m_Transform->m_scale.y)
				{
					m_Transform->m_scale.x = m_Transform->m_scale.y * ScaleRatio.y;
				}
			}
			change |= ImGui::DragFloat("Rotation", &m_Transform->m_rotation, 1.0);
		}

		ImGui::SeparatorText("Opacity");
		{
			change |= ImGui::DragFloat("Opacity", &m_Transform->m_opacity, 0.001, 0.0, 1.0);
		}

		ImGui::SeparatorText("Color");
		{
			ImGuiColorEditFlags base_flags = ImGuiColorEditFlags_None;
			change |= ImGui::ColorEdit4("Color", (float*)m_Transform->m_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | base_flags);
		}


		if (m_SelectObject.size() && change) // 만약 값이 변경되었으면 
		{
			// 시퀸서의 현재 프레임 위치에 키값이 있으면 해당 키값을 변경된 값으로 바꿔주고 
			// 키값이 없으면 새로운 키를 세팅해준다.
			SetTransform((*m_SelectObject.begin()).second, m_Transform);
		}
	}

	ImGui::End();
}

void UITransform::SetTransform(GameObject* obj, Transform value)
{
	SQ_Key* key = ((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->SetCurrentKey(obj);
	if (key != nullptr)
	{
		key->aniValue.TransformValue = value;
		key->aniValue.bezierIndex = ((UIBezierController*)g_UIManager->GetUI(UI_TYPE::BEZIERCONTROLLER))->GetBezierIndex();
		key->aniValue.B1 = ((UIBezierController*)g_UIManager->GetUI(UI_TYPE::BEZIERCONTROLLER))->GetB1();
		key->aniValue.B2 = ((UIBezierController*)g_UIManager->GetUI(UI_TYPE::BEZIERCONTROLLER))->GetB2();
	}
}

void UITransform::Update()
{

}

void UITransform::Input()
{

}