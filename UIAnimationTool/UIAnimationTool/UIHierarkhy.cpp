#include "stdafx.h"
#include "UIManager.h"
#include "UIHierarkhy.h"
#include "Tree.h"
#include "GameObject.h"
#include "InputManager.h"
#include "UISequencer.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_neo_internal.h"
#include "imgui/imgui_neo_sequencer.h"

UIHierarkhy::UIHierarkhy(UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix) :UIBase(manager, name, pos, size, posFix)
{
	m_Tree = new Tree<GameObject*>(new GameObject("Screen"));
	m_Dragging = false;
	m_Clicked = false;
	m_type = UI_TYPE::HIERARKHY;
}

UIHierarkhy::~UIHierarkhy()
{
	ClearAllNodes();
}

Tree<GameObject*>* UIHierarkhy::GetTreeRoot()
{
	return m_Tree;
}

void UIHierarkhy::AddNode(class GameObject* object)
{
	TreeNode<GameObject*>* leaf = new TreeNode<GameObject*>(object);
	m_Tree->GetRoot()->AddChild(leaf);
}

vector<TreeNode<class GameObject*>*> UIHierarkhy::GetAllNodes()
{
	return m_Tree->GetAllNode();
}

void UIHierarkhy::ClearAllNodes()
{
	m_Tree->Clear();
	m_Tree = new Tree<GameObject*>(new GameObject("Screen"));
}

void UIHierarkhy::RenderTree(TreeNode<GameObject*>* node)
{
	ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
	ImGuiTreeNodeFlags leaf_flags = base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	ImGuiTreeNodeFlags flags;


	if (node->GetChild().size())
		flags = base_flags;
	else
		flags = leaf_flags;

	/*if (node->GetData()->GetSelected())
		flags |= ImGuiTreeNodeFlags_Selected;*/

	if (g_UIManager->IsSelected(node->GetData()->GetInstanceID()))
		flags |= ImGuiTreeNodeFlags_Selected;

	bool node_open = ImGui::TreeNodeEx(node->GetData()->GetName().c_str(), flags, node->GetData()->GetName().c_str());


	if (ImGui::BeginDragDropSource())
	{
		int id = node->GetData()->GetInstanceID();
		ImGui::SetDragDropPayload("_TREENODE", &id, sizeof(int));
		ImGui::Text(node->GetData()->GetName().c_str());
		ImGui::EndDragDropSource();
	}


	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("_TREENODE"))
		{
			int id = *(const int*)payload->Data;
			TreeNode<class GameObject*>* cnode = m_Tree->SearchNode(id);
			m_Tree->MoveNode(node, cnode);
		}

		ImGui::EndDragDropTarget();
	}


	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		g_UIManager->SelectObject(node->GetData(), true);
		/*if (!ImGui::GetIO().KeyCtrl)
		{
			g_UIManager->SelectObject(node->GetData(), true);
		}
		else
		{
			g_UIManager->SelectObject(node->GetData());
		}*/
	}

	if (node->GetChild().size() && node_open)
	{
		for (int i = 0; i < node->GetChild().size(); i++)
		{
			RenderTree(node->GetChild()[i]);
		}
		ImGui::TreePop();
	}
}

void UIHierarkhy::Render()
{
	bool temp = true;
	ImVec2 temp2 = ImVec2(0, 0);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove;
	bool open = true;
	ImGui::Begin("Hierarkhy", &open, window_flags);
	UpdateWindowInfo();

	RenderTree(m_Tree->GetRoot());
	ImGui::End();

	/*if (ImGui::TreeNode("Hierarkhy"))
	{
		RenderTree(m_Tree->GetRoot());
		ImGui::TreePop();
	}*/

	/*ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	ImGuiTreeNodeFlags leaf_flags = base_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	if(ImGui::TreeNode("Hierarkhy"))
	{
		int index = 0;
		for (int i = 0; i < 2; i++)
		{
			m_Clicked = -1;
			bool is_selected = (m_Selected & (1 << i)) != 0;
			if (is_selected)
			{
				base_flags |= ImGuiTreeNodeFlags_Selected;
			}
			else
			{
				base_flags &= ~ImGuiTreeNodeFlags_Selected;
			}

			bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)i, base_flags, "1234");

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				m_Clicked = index;

			if (node_open)
			{
				ImGui::TreeNodeEx((void*)(intptr_t)i, leaf_flags, "qwer");

				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
					m_Clicked = index;

				ImGui::TreePop();
			}
		}

		ImGui::TreePop();
	}*/
}

void UIHierarkhy::Update()
{
	if (IsFocus())
	{
		if (g_InputManager->KeyPress(DIK_LCONTROL) && g_InputManager->KeyDown(DIK_C))
		{

		}
		//if (g_InputManager->KeyDown(DIK_DELETE))
		//{
		//	map<int, GameObject*> maps = g_UIManager->GetSelectedObject();
		//	GameObject* obj = (*maps.begin()).second;

		//	g_UIManager->DeleteObject(obj);

		//	//// 시퀸서에서 먼저 삭제해주고
		//	//((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->DeleteGroup(obj);

		//	//// 오브젝트 리스트에서도 삭제해주고
		//	//g_UIManager->DeleteObject(obj);

		//	//// 노드에서 삭제해준다.
		//	//TreeNode<GameObject*>* selectednode = m_Tree->SearchNode(obj->GetInstanceID());
		//	//m_Tree->DeleteNode(selectednode->GetData());

		//	g_UIManager->ClearObjectSelect();
		//}
	}
}

void UIHierarkhy::Input()
{
}
