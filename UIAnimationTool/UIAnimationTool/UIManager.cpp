#include "stdafx.h"
#include "UIManager.h"

#include "stdafx.h"
#include "GameObject.h"
#include "UIBase.h"
#include "UIHierarkhy.h"
#include "UIScreen.h"
#include "UISequencer.h"
#include "UITransform.h"
#include "Transform.h"
#include "Engine.h"
#include "FileManager.h"
#include "Input.h"
#include "ObjectManager.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_neo_internal.h"
#include "imgui/imgui_neo_sequencer.h"

UIManager* g_UIManager = new UIManager();

UIManager::UIManager()
{
	m_CopiedObj = nullptr;
	m_CopiedKey = nullptr;
	m_CanvasSize[0] = 1024;
	m_CanvasSize[1] = 768;
	m_CanvasRatio = (float)1024 / (float)768;
	m_CanvasRatioType = 0;
}

UIManager::~UIManager()
{
	ClearAllObjects();

	for (pair<UI_TYPE, UIBase*> itr : m_UIMap)
	{
		delete itr.second;
	}

	m_UIMap.clear();
}

GameObject* UIManager::GetCopiedObj()
{
	return m_CopiedObj;
}

SQ_Key* UIManager::GetCopiedKey()
{
	return m_CopiedKey;
}

int* UIManager::GetCanvasSize()
{
	return m_CanvasSize;
}

void UIManager::Add(UI_TYPE type, UIBase* ui)
{
	if (m_UIMap.find(type) == m_UIMap.end())
	{
		m_UIMap[type] = ui;
	}
}

void UIManager::Delete(UI_TYPE type)
{
	if (m_UIMap.find(type) != m_UIMap.end())
	{
		m_UIMap.erase(type);
	}
}

UIBase* UIManager::GetUI(UI_TYPE type)
{
	if (m_UIMap.find(type) == m_UIMap.end())
		return nullptr;

	return m_UIMap[type];
}

UIBase* UIManager::GetFocusedUI()
{
	for (pair<UI_TYPE, UIBase*> u : m_UIMap)
	{
		if (u.second->IsFocus())
			return u.second;
	}

	return nullptr;
}

void UIManager::ClearAllObjects()
{
	//모든 오브젝트들을 다 삭제해준다.
	((UISequencer*)GetUI(UI_TYPE::SEQUENCER))->ClearAllObjects();
	((UIHierarkhy*)GetUI(UI_TYPE::HIERARKHY))->ClearAllNodes();
	m_SelectObjectMap.clear();
	m_ObjectList.clear();
	g_ObjectManager->ClearAction();
}

void UIManager::DeleteObject(GameObject* obj, bool addHistory)
{
	if (addHistory)
	{
		g_ObjectManager->PushAction(((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetGroup(obj), sizeof(SQ_Group), ACTION_TYPE::ACTION_DELETE_GROUP);
		g_ObjectManager->PushAction(obj, sizeof(GameObject), ACTION_TYPE::ACTION_DELETE_OBJECT);
	}

	// 시퀸서에서 먼저 삭제해주고
	((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->DeleteGroup(obj);

	// 노드에서 삭제해준다.
	Tree<GameObject*>* tree = ((UIHierarkhy*)g_UIManager->GetUI(UI_TYPE::HIERARKHY))->GetTreeRoot();
	TreeNode<GameObject*>* selectednode = tree->SearchNode(obj->GetInstanceID());
	tree->DeleteNode(selectednode->GetData());

	// 오브젝트 리스트에서도 삭제해주고
	for (vector<GameObject*>::iterator itr = m_ObjectList.begin(); itr != m_ObjectList.end(); itr++)
	{
		if ((*itr)->GetInstanceID() == obj->GetInstanceID())
		{
			//Action Test
			if (!addHistory)
				delete obj;

			m_ObjectList.erase(itr);
			break;
		}
	}
}


void UIManager::Render()
{
	if (m_UIMap.size() <= 0)
		return;

	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = 1.6;

	RenderStart();
	RenderMenuBar();

	for (pair<UI_TYPE, UIBase*> itr : m_UIMap)
	{
		itr.second->Render();
	}

	RenderEnd();
}

void UIManager::Update()
{
	if (m_UIMap.size() <= 0)
		return;

	for (pair<UI_TYPE, UIBase*> itr : m_UIMap)
	{
		itr.second->Update();
	}

	UIBase* ui = GetFocusedUI();
	if (ui != nullptr)
	{
		if (ui->GetType() != UI_TYPE::SEQUENCER && g_InputManager->KeyDown(DIK_DELETE))
		{
			map<int, GameObject*> maps = GetSelectedObject();
			if (maps.size())
			{
				GameObject* obj = (*maps.begin()).second;
				DeleteObject(obj);
				ClearObjectSelect();
			}
		}
	}

	if (g_InputManager->KeyPress(DIK_LCONTROL) && g_InputManager->KeyDown(DIK_Z))
	{
		PayloadData* payload = g_ObjectManager->PopAction();
		if (payload != nullptr)
		{
			if (payload->_type == ACTION_DELETE_OBJECT)
			{
				PayloadData* payload2 = g_ObjectManager->PopAction();
				SQ_Group* group = (SQ_Group*)payload2->_data;

				GameObject* obj = (GameObject*)payload->_data;
				AddObject(obj, false);

				vector<SQ_Key> keys = group->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKeys();
				for (int i = 1; i < keys.size(); i++)
				{
					((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->AddKey(obj, TIMELINE_TYPE::TL_GROUP, &keys[i], 1);
				}

				delete payload2;
			}
			else if (payload->_type == ACTION_DELETE_KEY)
			{
				KeyActionData kdata;
				memcpy(&kdata, payload->_data, sizeof(KeyActionData));
				((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->AddKey(kdata.ObjectID, kdata.TimelineType, &kdata.key, 1);
			}
			else if (payload->_type == ACTION_CREATE_OBJECT)
			{
				GameObject* obj = (GameObject*)payload->_data;
				DeleteObject(obj, false);

			}
			else if (payload->_type == ACTION_CREATE_KEY)
			{
				KeyActionData kdata;
				memcpy(&kdata, payload->_data, sizeof(KeyActionData));
				((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetGroup(kdata.ObjectID)->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->DeleteKey(kdata.KeyIndex);
			}
			else if (payload->_type == ACTION_CHANGE_TRANSFORM)
			{
				KeyActionData kdata;
				memcpy(&kdata, payload->_data, sizeof(KeyActionData));
				SQ_Key* key = ((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetGroup(kdata.ObjectID)->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKey_Index(kdata.KeyIndex);
				if (key != nullptr)
					memcpy(key, &kdata.key, sizeof(SQ_Key));
			}

			delete payload;
		}
	}

	if (g_InputManager->KeyPress(DIK_LCONTROL) && g_InputManager->KeyDown(DIK_C))
	{
		if (GetFocusedUI()->GetType() == UI_TYPE::SEQUENCER)
		{
			//선택된 키가 존재하면 가장 마지막꺼만 복사한다.
			if (GetSelectedObject().size())
			{
				vector<SQ_Key> keys = ((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetGroup((*GetSelectedObject().begin()).second)->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetSelectedKey();
				if (keys.size())
				{
					KeyActionData kdata;
					kdata.key = keys[keys.size() - 1];
					kdata.KeyIndex = keys.size() - 1;
					kdata.ObjectID = (*GetSelectedObject().begin()).second->GetInstanceID();
					kdata.TimelineType = TIMELINE_TYPE::TL_GROUP;
					g_ObjectManager->Copy(&kdata, sizeof(KeyActionData), COPY_TYPE::COPY_KEY);
				}
			}
		}
		else
		{
			g_ObjectManager->Copy((*GetSelectedObject().begin()).second, sizeof(GameObject), COPY_TYPE::COPY_OBJECT);
		}
	}

	if (g_InputManager->KeyPress(DIK_LCONTROL) && g_InputManager->KeyDown(DIK_V))
	{
		PayloadData* payload = g_ObjectManager->Paste();
		if (payload != nullptr)
		{
			if (GetFocusedUI()->GetType() == UI_TYPE::SEQUENCER)
			{
				if (payload->_type == COPY_TYPE::COPY_KEY)
				{
					if (GetSelectedObject().size())
					{
						KeyActionData kdata;
						memcpy(&kdata, payload->_data, sizeof(KeyActionData));
						((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->SetCurrentKey((*GetSelectedObject().begin()).second, kdata.key);
					}
				}
			}
			else
			{
				if (payload->_type == COPY_TYPE::COPY_OBJECT)
				{
					GameObject* oriObject = (GameObject*)payload->_data;
					GameObject* cpyObject = new GameObject(oriObject);
					AddObject(cpyObject);

					vector<SQ_Key> keys = ((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetGroup(oriObject->GetInstanceID())->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKeys();
					for (int i = 1; i < keys.size(); i++)
					{
						((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->AddKey(cpyObject, TIMELINE_TYPE::TL_GROUP, &keys[i], 1);
					}
				}
			}
			//delete payload;
		}
	}

	if (g_InputManager->MouseLDown() && ui != nullptr && (ui->GetType() == UI_TYPE::SCREEN || ui->GetType() == UI_TYPE::TRANSFORM) && GetSelectedObject().size())
	{
		int index = 0;
		SQ_Key* key = ((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetCurrentKey(&index);

		if (key != nullptr)
		{
			KeyActionData kdata;
			memcpy(&kdata.key, key, sizeof(SQ_Key));
			kdata.ObjectID = (*GetSelectedObject().begin()).second->GetInstanceID();
			kdata.TimelineType = TIMELINE_TYPE::TL_GROUP;
			kdata.KeyIndex = index;

			g_ObjectManager->PushAction(&kdata, sizeof(KeyActionData), ACTION_TYPE::ACTION_CHANGE_TRANSFORM);
		}
	}
}

void UIManager::Input()
{
}

void UIManager::RenderMenuBar()
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	bool flag = true;

	//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace Demo", &flag, window_flags);
	//ImGui::PopStyleVar();
	ImGui::PopStyleVar(2);


	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	}

	bool temp1 = false;
	bool temp2 = false;

	if (ImGui::BeginMenuBar())
	{

		if (ImGui::BeginMenu("Menu"))
		{
			ImGui::Separator();

			if (ImGui::MenuItem("Save", NULL, false))
			{
				g_FileManager->SaveFile();
			}
			if (ImGui::MenuItem("Load", NULL, false))
			{
				if (g_FileManager->LoadFile())
				{
					ClearAllObjects();
					Json::Value& jValue = g_FileManager->GetLoadData();

					if (jValue.isMember("maxframe"))
						((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->SetEndFrame(jValue["maxframe"].asFloat());


					int count = jValue["objects"].size();
					for (int i = 0; i < count; i++)
					{
						string str = jValue["objects"][i]["objname"].asCString();
						GameObject* rectObject = new GameObject(str.c_str());
						m_ObjectList.push_back(rectObject);
						((UIHierarkhy*)GetUI(UI_TYPE::HIERARKHY))->AddNode(rectObject);
						((UISequencer*)GetUI(UI_TYPE::SEQUENCER))->AddObject(rectObject);

						//Texture 로드
						DWORD width;
						DWORD height;
						char filename[125] = { 0 };
						char filepath[1024] = { 0 };

						if (jValue["objects"][i].isMember("filename") && jValue["objects"][i].isMember("filepath"))
						{
							strcpy(filename, jValue["objects"][i]["filename"].asCString());
							strcpy(filepath, jValue["objects"][i]["filepath"].asCString());
							LPDIRECT3DTEXTURE9 tex = g_FileManager->LoadTexture(&width, &height, filepath);
							if (tex)
							{
								rectObject->SetTexture(tex, width, height, filename, filepath);
							}
						}
						//Texture 로드

						int keycount = jValue["objects"][i]["keys"].size();
						for (int j = 0; j < keycount; j++)
						{
							Transform tf;
							Json::Value key = jValue["objects"][i]["keys"][j];

							tf.m_position = D3DXVECTOR2(key["position"]["value"][0].asFloat(), key["position"]["value"][1].asFloat());
							if (key.isMember("size"))
								tf.m_size = D3DXVECTOR2(key["size"]["value"][0].asFloat(), key["size"]["value"][1].asFloat());
							tf.m_scale = D3DXVECTOR2(key["scale"]["value"][0].asFloat(), key["scale"]["value"][1].asFloat());
							tf.m_rotation = key["rotation"]["value"][0].asFloat();
							tf.m_opacity = key["opacity"]["value"][0].asFloat();
							tf.m_color = D3DXVECTOR4(key["color"]["value"][0].asFloat(), key["color"]["value"][1].asFloat(), \
								key["color"]["value"][2].asFloat(), key["color"]["value"][3].asFloat());

							int bIndex = 0;
							if (key["position"].isMember("bezierindex"))
								bIndex = key["position"]["bezierindex"].asInt();

							SQ_Key keys = SQ_Key(key["position"]["time"].asFloat(), tf, bIndex, D3DXVECTOR4(key["position"]["B1"]["x"].asFloat(), key["position"]["B1"]["y"].asFloat(), key["position"]["B2"]["x"].asFloat(), key["position"]["B2"]["y"].asFloat()));
							((UISequencer*)GetUI(UI_TYPE::SEQUENCER))->AddKey(rectObject, TIMELINE_TYPE::TL_GROUP, &keys, 1);
						}
					}
				}
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("New Screen", NULL, false))
			{
				ClearAllObjects();
			}
			if (ImGui::MenuItem("Rectangle", NULL, false))
			{
				CreateRectangle();
			}


			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Option"))
		{
			if (ImGui::BeginMenu("Canvas Option"))
			{
				ImGui::SeparatorText("Canvas Ratio");
				if (ImGui::RadioButton("4 : 3", &m_CanvasRatioType, 0))
				{
					m_CanvasSize[0] = 1024;
					m_CanvasSize[1] = 768;
					m_CanvasRatio = (float)1024 / (float)768;
				}
				ImGui::SameLine();

				if (ImGui::RadioButton("16 : 9", &m_CanvasRatioType, 1))
				{
					m_CanvasSize[0] = 1376;
					m_CanvasSize[1] = 774;
					m_CanvasRatio = (float)1376 / (float)774;
				}

				ImGui::SeparatorText("Canvas Size");

				int oldSize[2];
				memcpy(oldSize, m_CanvasSize, sizeof(int) * 2);
				if (ImGui::DragInt2("Canvas Size", m_CanvasSize, 1.0))
				{
					if (oldSize[0] != m_CanvasSize[0])
						m_CanvasSize[1] = m_CanvasSize[0] / m_CanvasRatio;
					else if (oldSize[1] != m_CanvasSize[1])
						m_CanvasSize[0] = m_CanvasSize[1] * m_CanvasRatio;
				}
				ImGui::EndMenu();
			}
			ImGui::SeparatorText("ClearZoom");
			if (ImGui::MenuItem("ClearZoom", NULL, false))
			{
				g_InputManager->ClearMouseWheel();
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
	ImGui::End();
}

void UIManager::CreateRectangle()
{
	GameObject* rectObject = new GameObject(CheckObjectName("Rectangle"));
	m_ObjectList.push_back(rectObject);
	((UIHierarkhy*)GetUI(UI_TYPE::HIERARKHY))->AddNode(rectObject);
	((UISequencer*)GetUI(UI_TYPE::SEQUENCER))->AddObject(rectObject);

	SQ_Key keys = SQ_Key(0, rectObject->GetTransform(), 0, D3DXVECTOR4(0, 0, 1, 1));
	((UISequencer*)GetUI(UI_TYPE::SEQUENCER))->AddKey(rectObject, TIMELINE_TYPE::TL_GROUP, &keys, 1);

	g_ObjectManager->PushAction(rectObject, sizeof(GameObject), ACTION_TYPE::ACTION_CREATE_OBJECT);

}

void UIManager::AddObject(GameObject* obj, bool addHistory)
{
	m_ObjectList.push_back(obj);
	((UIHierarkhy*)GetUI(UI_TYPE::HIERARKHY))->AddNode(obj);
	((UISequencer*)GetUI(UI_TYPE::SEQUENCER))->AddObject(obj);

	SQ_Key keys = SQ_Key(0, obj->GetTransform(), 0, D3DXVECTOR4(0, 0, 1, 1));
	((UISequencer*)GetUI(UI_TYPE::SEQUENCER))->AddKey(obj, TIMELINE_TYPE::TL_GROUP, &keys, 1);

	if (addHistory)
		g_ObjectManager->PushAction(obj, sizeof(GameObject), ACTION_TYPE::ACTION_CREATE_OBJECT);
}

void UIManager::ClearObjectSelect()
{
	for (GameObject* t : m_ObjectList)
	{
		t->SetSelected(false);
	}
	m_SelectObjectMap.clear();
}

void UIManager::SelectObject(GameObject* obj, bool clear)
{
	if (clear)
	{
		ClearObjectSelect();
	}

	if (m_SelectObjectMap.find(obj->GetInstanceID()) != m_SelectObjectMap.end())
	{
		obj->SetSelected(false);
		m_SelectObjectMap.erase(obj->GetInstanceID());
	}
	else
	{
		obj->SetSelected(true);
		m_SelectObjectMap[obj->GetInstanceID()] = obj;
	}
}

bool UIManager::IsSelected(int instanceID)
{
	if (m_SelectObjectMap.find(instanceID) != m_SelectObjectMap.end())
	{
		return true;
	}
	return false;
}

map<int, GameObject*> UIManager::GetSelectedObject()
{
	return m_SelectObjectMap;
}




void UIManager::CreateCircle()
{

}

void UIManager::SaveData()
{

}


void UIManager::RenderStart()
{
	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void UIManager::RenderEnd()
{
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

	ImGuiIO& io = ImGui::GetIO();
	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}
