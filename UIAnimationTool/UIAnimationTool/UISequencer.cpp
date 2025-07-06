#include "stdafx.h"
#include "UIManager.h"
#include "UISequencer.h"
#include "Engine.h"
#include "Transform.h"
#include "GameObject.h"
#include "UIBezierController.h"
#include "UIHierarkhy.h"
#include "InputManager.h"
#include "ObjectManager.h"

#include "UIAnimationTool.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_neo_internal.h"
#include "imgui/imgui_neo_sequencer.h"


AnimationInfo::AnimationInfo()
{
    Time = 0;
    TransformValue = Transform();
    B1 = D3DXVECTOR2(0, 0);
    B2 = D3DXVECTOR2(1, 1);
    bezierIndex = 0;
}

AnimationInfo::AnimationInfo(float t, Transform value, int bIndex, D3DXVECTOR4 bezier)
{
    Time = t;
    TransformValue = Transform(value);
    B1 = D3DXVECTOR2(bezier.x, bezier.y);
    B2 = D3DXVECTOR2(bezier.z, bezier.w);
    bezierIndex = bIndex;
}

AnimationInfo::AnimationInfo(const AnimationInfo& value)
{
    Time = value.Time;
    TransformValue = Transform(value.TransformValue);
    B1 = value.B1;
    B2 = value.B2;
    bezierIndex = value.bezierIndex;
}
AnimationInfo::~AnimationInfo()
{
    //delete TransformValue;
}


bool cmp(SQ_Key a, SQ_Key b) {
    return a.time < b.time;
}

#pragma region SQ_Key
SQ_Key::SQ_Key()
{
    this->time = 0;
    //this->aniValue = nullptr;
    isSelected = false;
}

SQ_Key::SQ_Key(int time, Transform value, int bIndex, D3DXVECTOR4 bezier)
{
    this->time = time;
    this->aniValue = AnimationInfo(time, value, bIndex, bezier);
    isSelected = false;
}

SQ_Key::SQ_Key(int time, Transform value, int bIndex, D3DXVECTOR2 b1, D3DXVECTOR2 b2)
{
    this->time = time;
    this->aniValue = AnimationInfo(time, value, bIndex, D3DXVECTOR4(b1.x, b1.y, b2.x, b2.y));
    isSelected = false;
}

SQ_Key::SQ_Key(const SQ_Key& value)
{
    time = value.time;
    isSelected = value.isSelected;
    aniValue = value.aniValue;
}

//SQ_Key::SQ_Key(AnimationInfo* value)
//{
//    this->time = value->Time;
//    this->aniValue = AnimationInfo(value);
//}

SQ_Key::~SQ_Key()
{
    //delete aniValue;
}
#pragma endregion

#pragma region SQ_TimeLine

SQ_TimeLine::SQ_TimeLine(GameObject* _obejct)
{
    //this->name = _name;
    this->object = _obejct;
}

SQ_TimeLine::~SQ_TimeLine()
{
    keys.clear();
}

void SQ_TimeLine::Render()
{
    if (ImGui::BeginNeoTimelineEx(/*name.c_str()*/object->GetName().c_str()))
    {
        if (g_UIManager->IsSelected(object->GetInstanceID()))
        {
            if (!ImGui::IsNeoTimelineSelected())
            {
                ImGui::SetSelectedTimeline(/*name.c_str()*/object->GetName().c_str());
            }
        }

        if (ImGui::IsNeoTimelineSelected(ImGuiNeoTimelineIsSelectedFlags_NewlySelected))
        {
            g_UIManager->SelectObject(object, true);
        }

        //if (ImGui::IsNeoTimelineSelected())
        //{
        //    if(!g_UIManager->IsClicked(object->GetInstanceID()))
        //        g_UIManager->ClickObject(object, true);

        //    /*g_UIManager->ClearObjectClick();
        //    this->object->SetSelected(true);*/
        //}

        for (auto&& v : keys)
        {
            ImGui::NeoKeyframe(&v.time);

            if (ImGui::IsNeoKeyframeClick())
            {
#if sequencerfloat
                g_CurFrame = ImGui::GetNeoKeyframeClickValue() * FrameRatio;
#else
                g_CurFrame = ImGui::GetNeoKeyframeClickValue();
#endif
                g_UIManager->SelectObject(object, true);

                if (!ImGui::IsWindowFocused())
                    ImGui::SetWindowFocus();
            }

            v.isSelected = ImGui::IsNeoKeyframeSelected();

            if (ImGui::IsNeoKeyframeSelected())
            {


            }
            // Per keyframe code here
        }

        /*if (g_UIManager->GetUI(UI_TYPE::SEQUENCER)->IsFocus() && ImGui::IsNeoTimelineSelected() && deleteKey)
        {
            uint32_t count = ImGui::GetNeoKeyframeSelectionSize();
            ImGui::FrameIndexType* toRemove = new ImGui::FrameIndexType[count];
            ImGui::GetNeoKeyframeSelection(toRemove);

            for (int i = 0; i < count; i++)
            {
                int a = toRemove[i];
                DeleteKey(toRemove[i]);
            }

            ImGui::NeoClearSelection();
            deleteKey = false;
        }*/

        ImGui::EndNeoTimeLine();
    }
}

void SQ_TimeLine::AddKey(SQ_Key* key, int count)
{
    for (int i = 0; i < count; i++)
        keys.push_back(key[i]);

    sort(keys.begin(), keys.end(), cmp);
}

void SQ_TimeLine::DeleteKey()
{
    uint32_t count = ImGui::GetNeoKeyframeSelectionSize();
    ImGui::FrameIndexType* toRemove = new ImGui::FrameIndexType[count];
    ImGui::GetNeoKeyframeSelection(toRemove);
}


void SQ_TimeLine::DeleteKey(int index)
{
    // 0번 인덱스는 삭제 불가능
    if (index != 0)
        keys.erase(keys.begin() + index);
}

SQ_Key SQ_TimeLine::GetPreKey(float time)
{
    for (int i = 1; i < keys.size(); i++)
    {
        if (keys[i].time >= time)
            return keys[i - 1];
    }
    return keys[keys.size() - 1];
}

SQ_Key SQ_TimeLine::GetNextKey(float time)
{
    for (int i = 1; i < keys.size(); i++)
    {
        if (keys[i].time >= time)
            return keys[i];
    }
    return keys[keys.size() - 1];
}

SQ_Key* SQ_TimeLine::GetKey(int time)
{
    for (int i = 0; i < keys.size(); i++)
    {
        if (keys[i].time == time)
            return &keys[i];
    }

    return nullptr;
}

SQ_Key* SQ_TimeLine::GetKey(int time, int* out_index)
{
    *out_index = -1;
    for (int i = 0; i < keys.size(); i++)
    {
        if (keys[i].time == time)
        {
            *out_index = i;
            return &keys[i];
        }
    }

    return nullptr;
}

vector<SQ_Key> SQ_TimeLine::GetSelectedKey()
{
    vector<SQ_Key> v;
    for (int i = 0; i < keys.size(); i++)
    {
        if (keys[i].isSelected)
            v.push_back(keys[i]);
    }
    return v;
}

SQ_Key* SQ_TimeLine::GetKey_Index(int index)
{
    if (index >= 0 && index < keys.size())
    {
        return &keys[index];
    }
    return nullptr;
}
#pragma endregion


#pragma region SQ_Group
SQ_Group::SQ_Group(GameObject* _obejct)
{
    Open = false;
    this->object = _obejct;
}

SQ_Group::~SQ_Group()
{
    for (pair<int, SQ_TimeLine*>d : m_TimelineMap)
    {
        delete d.second;
    }
    m_TimelineMap.clear();
}

void SQ_Group::Render()
{
    if (m_TimelineMap.size() > 1)
    {
        if (ImGui::BeginNeoGroup(/*name.c_str()*/object->GetName().c_str(), &Open))
        {
            for (pair<int, SQ_TimeLine*>d : m_TimelineMap)
            {
                d.second->Render();
            }

            ImGui::EndNeoGroup();
        }
    }
    else
    {
        for (pair<int, SQ_TimeLine*>d : m_TimelineMap)
        {
            d.second->Render();
        }
    }

}

void SQ_Group::AddTimeLine(int tl_type)
{
    if (m_TimelineMap.find(tl_type) == m_TimelineMap.end())
    {
        m_TimelineMap[tl_type] = new SQ_TimeLine(object);
    }
}

void SQ_Group::AddKey(int tl_type, SQ_Key* key, int count)
{
    if (m_TimelineMap.find(tl_type) != m_TimelineMap.end())
    {
        m_TimelineMap[tl_type]->AddKey(key, count);
    }
}

SQ_TimeLine* SQ_Group::GetTimeLine(int tl_type)
{
    if (m_TimelineMap.find(tl_type) != m_TimelineMap.end())
    {
        return m_TimelineMap[tl_type];
    }
    return nullptr;
}

#pragma endregion

UISequencer::UISequencer(UIManager* manager, const char* name, D3DXVECTOR2 pos, D3DXVECTOR2 size, bool posFix) :UIBase(manager, name, pos, size, posFix)
{
    m_currentFrame = 0;
    m_startFrame = 0;
#if sequencerfloat
    m_endFrame = 500;
#else
    m_endFrame = 20;
#endif
    bool doDelete = false;

    RECT rt;
    gEngine->GetCurClientRect(rt);
    ImGui::SetNextWindowPos(ImVec2(rt.left + 20, rt.top + 20));
    m_Play = false;
    m_type = UI_TYPE::SEQUENCER;
}

UISequencer::~UISequencer()
{
    ClearAllObjects();
}

void UISequencer::Render()
{
    if (ImGui::BeginNeoSequencer("Sequencer", &m_currentFrame, &m_startFrame, &m_endFrame, { 0, 0 },
        ImGuiNeoSequencerFlags_AllowLengthChanging |
        ImGuiNeoSequencerFlags_EnableSelection |
        ImGuiNeoSequencerFlags_Selection_EnableDragging |
        ImGuiNeoSequencerFlags_Selection_EnableDeletion))
    {
        UpdateWindowInfo();

        if (ImGui::IsNeoHoldingCurFrame())
        {
#if sequencerfloat
            g_CurFrame = m_currentFrame * FrameRatio;
#else
            g_CurFrame = m_currentFrame;
#endif
            ImGui::SetTooltip("%0.2f", g_CurFrame);
        }
        else
        {
#if sequencerfloat
            m_currentFrame = round(g_CurFrame / FrameRatio);
#else
            m_currentFrame = g_CurFrame;
#endif
        }

        auto nodes = ((UIHierarkhy*)g_UIManager->GetUI(UI_TYPE::HIERARKHY))->GetAllNodes();
        if (nodes.size())
        {
            for (int i = 0; i < nodes.size(); i++)
            {
                int id = nodes[i]->GetData()->GetInstanceID();
                if (m_SqMap.find(id) != m_SqMap.end())
                {
                    m_SqMap[id]->Render();
                }
            }
        }


        if (IsFocus())
        {
            if (g_InputManager->KeyDown(DIK_DELETE))
            {
                for (pair<int, SQ_Group*>d : m_SqMap)
                {
                    vector<SQ_Key> keys = d.second->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKeys();
                    for (int i = keys.size() - 1; i >= 0; i--)
                    {
                        KeyActionData kdata;
                        kdata.ObjectID = d.second->object->GetInstanceID();
                        kdata.TimelineType = TIMELINE_TYPE::TL_GROUP;
                        kdata.KeyIndex = i;

                        if (keys[i].isSelected)
                        {
                            d.second->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->DeleteKey(i);
                            kdata.key = keys[i];
                            g_ObjectManager->PushAction(&kdata, sizeof(KeyActionData), ACTION_TYPE::ACTION_DELETE_KEY);
                        }
                    }
                }
                ImGui::NeoClearSelection();
            }
        }

        ImGui::EndNeoSequencer();
    }
}

void UISequencer::Update()
{
    if (spaceKey)
    {
        if (m_Play)
            m_Play = false;
        else
        {
            m_Play = true;
            m_PlayTime = clock();
            m_LastFrame = m_PlayTime;
        }
        spaceKey = false;
    }

    int curtime = clock();
    if (m_Play/* && (m_LastFrame + g_AniFrameRate) < curtime*/)
    {

        g_CurFrame = (curtime - m_PlayTime) / 1000.0;

        if (m_currentFrame >= m_endFrame)
        {
            m_currentFrame = 0;
            g_CurFrame = 0;
            m_PlayTime = clock();
        }
    }
    else
    {
        if (IsFocus())
        {
            if (g_InputManager->KeyPress(DIK_RIGHTARROW, 200))
            {
                if (g_CurFrame < m_endFrame * FrameRatio)
                    g_CurFrame += 0.01;

                g_CurFrame = round(g_CurFrame * 100) / 100;
            }
            else if (g_InputManager->KeyPress(DIK_LEFTARROW, 200))
            {
                if (g_CurFrame > 0)
                    g_CurFrame -= 0.01;

                g_CurFrame = round(g_CurFrame * 100) / 100;
            }
        }
    }

    //현재 생성되어있는 모든 오브젝트들의 Transform을 현재 세팅되어있는 프레임에 맞춰서 계산해서 변경해준다.
    for (pair<int, SQ_Group*>d : m_SqMap)
    {
        vector<SQ_Key> keys = d.second->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKeys();
        SQ_Key preKey = d.second->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetPreKey(m_currentFrame);
        SQ_Key nextKey = d.second->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetNextKey(m_currentFrame);

        Transform curTransform = CalcTransform(m_currentFrame, &preKey, &nextKey);
        d.second->object->GetTransform()->m_position = curTransform.m_position;
        d.second->object->GetTransform()->m_size = curTransform.m_size;
        d.second->object->GetTransform()->m_scale = curTransform.m_scale;
        d.second->object->GetTransform()->m_rotation = curTransform.m_rotation;
        d.second->object->GetTransform()->m_opacity = curTransform.m_opacity;
        d.second->object->GetTransform()->m_color = curTransform.m_color;

        if (g_UIManager->IsSelected(d.second->object->GetInstanceID()))
            ((UIBezierController*)g_UIManager->GetUI(UI_TYPE::BEZIERCONTROLLER))->SetBezierValue(nextKey.aniValue.bezierIndex, D3DXVECTOR4(nextKey.aniValue.B1.x, nextKey.aniValue.B1.y, nextKey.aniValue.B2.x, nextKey.aniValue.B2.y));
    }
}

Transform UISequencer::CalcTransform(float time, SQ_Key* preKey, SQ_Key* nextKey)
{
    Transform result;
    D3DXVECTOR2 Bezier;
    float t = (time - preKey->time) / (nextKey->time - preKey->time);

    if (time - preKey->time <= 0)
    {
        Bezier = D3DXVECTOR2(0, 0);
    }
    else if (nextKey->time - preKey->time <= 0)
    {
        Bezier = D3DXVECTOR2(1, 1);
    }
    else
    {
        Bezier = CalcBezier(nextKey->aniValue.B1, nextKey->aniValue.B2, t);
    }

    Transform preVal = preKey->aniValue.TransformValue;
    Transform nextVal = nextKey->aniValue.TransformValue;

    result.m_position = preVal.m_position + (nextVal.m_position - preVal.m_position) * Bezier.y;
    result.m_size = preVal.m_size + (nextVal.m_size - preVal.m_size) * Bezier.y;
    result.m_scale = preVal.m_scale + (nextVal.m_scale - preVal.m_scale) * Bezier.y;
    result.m_rotation = preVal.m_rotation + (nextVal.m_rotation - preVal.m_rotation) * Bezier.y;
    result.m_opacity = preVal.m_opacity + (nextVal.m_opacity - preVal.m_opacity) * Bezier.y;
    result.m_color = preVal.m_color + (nextVal.m_color - preVal.m_color) * Bezier.y;

    return result;
}

D3DXVECTOR2 UISequencer::CalcBezier(D3DXVECTOR2 B1, D3DXVECTOR2 B2, float t)
{
    D3DXVECTOR2 Start = D3DXVECTOR2(0, 0);
    D3DXVECTOR2 End = D3DXVECTOR2(1, 1);

    return (1 - t) * (1 - t) * (1 - t) * Start + 3 * (1 - t) * (1 - t) * t * B1 + 3 * (1 - t) * t * t * B2 + t * t * t * End;
}

void UISequencer::Input()
{

}

// 현재 선택되어있는 그룹이 존재하고
// 현재 프레임에 그 그룹의 키값이 존재하면 해당 키값을 리턴
SQ_Key* UISequencer::GetCurrentKey()
{
    map<int, GameObject*> map = g_UIManager->GetSelectedObject();
    if (map.size() && m_SqMap.count((*map.begin()).second->GetInstanceID()))
    {
        //일단은 선택된 오브젝트들 중에서 첫번째꺼 하나만 기능한다
        SQ_Key* key = m_SqMap[(*map.begin()).second->GetInstanceID()]->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKey(m_currentFrame);

        return key;
    }

    return nullptr;
}

SQ_Key* UISequencer::GetCurrentKey(int* index)
{
    map<int, GameObject*> map = g_UIManager->GetSelectedObject();
    if (map.size() && m_SqMap.count((*map.begin()).second->GetInstanceID()))
    {
        //일단은 선택된 오브젝트들 중에서 첫번째꺼 하나만 기능한다
        SQ_Key* key = m_SqMap[(*map.begin()).second->GetInstanceID()]->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKey(m_currentFrame, index);
        return key;
    }

    return nullptr;
}

// 현재 선택되어있는 그룹이 존재하고
// 현재 프레임에 그 그룹의 키값이 존재하면 해당 키값을 리턴하고
// 현재 프레임에 그 그룹의 키값이 없으면 새롭케 키값을 생성해준다.
SQ_Key* UISequencer::SetCurrentKey(GameObject* obj)
{
    SQ_Key* key = m_SqMap[obj->GetInstanceID()]->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKey(m_currentFrame);

    if (key == nullptr)
    {
        SQ_Key keys = SQ_Key(m_currentFrame, obj->GetTransform(), 0, D3DXVECTOR4(0, 0, 1, 1));
        AddKey(obj, TIMELINE_TYPE::TL_GROUP, &keys, 1);

        KeyActionData kdata;
        kdata.ObjectID = obj->GetInstanceID();
        kdata.TimelineType = TIMELINE_TYPE::TL_GROUP;
        kdata.KeyIndex = m_SqMap[obj->GetInstanceID()]->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKeys().size() - 1;
        kdata.key = keys;

        g_ObjectManager->PushAction(&kdata, sizeof(KeyActionData), ACTION_TYPE::ACTION_CREATE_KEY);
    }

    return key;
}

void UISequencer::SetCurrentKey(GameObject* obj, SQ_Key keydata)
{
    SQ_Key* key = m_SqMap[obj->GetInstanceID()]->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKey(m_currentFrame);
    if (key == nullptr)
    {
        keydata.time = m_currentFrame;
        AddKey(obj, TIMELINE_TYPE::TL_GROUP, &keydata, 1);
    }
    else
    {
        memcpy(key, &keydata, sizeof(SQ_Key));
        key->time = m_currentFrame;
    }
}


void UISequencer::AddKey(GameObject* object, int tl_type, SQ_Key* key, int count)
{
    if (m_SqMap.find(object->GetInstanceID()) != m_SqMap.end())
    {
        m_SqMap[object->GetInstanceID()]->AddKey(tl_type, key, count);
    }
}

void UISequencer::AddKey(int instanceID, int tl_type, SQ_Key* key, int count)
{
    if (m_SqMap.find(instanceID) != m_SqMap.end())
    {
        m_SqMap[instanceID]->AddKey(tl_type, key, count);
    }
}

void UISequencer::AddTimeLine(GameObject* object, int tl_type)
{
    if (m_SqMap.find(object->GetInstanceID()) != m_SqMap.end())
    {
        ((SQ_Group*)m_SqMap[object->GetInstanceID()])->AddTimeLine(tl_type);
    }
}

void UISequencer::AddObject(GameObject* object)
{
    if (m_SqMap.find(object->GetInstanceID()) == m_SqMap.end())
    {
        SQ_Group* group = new SQ_Group(object);
        m_SqMap[object->GetInstanceID()] = group;
        group->AddTimeLine(TIMELINE_TYPE::TL_GROUP);
    }
}

SQ_Group* UISequencer::GetGroup(GameObject* object)
{
    if (m_SqMap.find(object->GetInstanceID()) != m_SqMap.end())
    {
        return m_SqMap[object->GetInstanceID()];
    }
    return nullptr;
}

SQ_Group* UISequencer::GetGroup(int instanceID)
{
    if (m_SqMap.find(instanceID) != m_SqMap.end())
    {
        return m_SqMap[instanceID];
    }
    return nullptr;
}


void UISequencer::DeleteGroup(GameObject* object)
{
    for (map<int, SQ_Group*>::iterator itr = m_SqMap.begin(); itr != m_SqMap.end(); itr++)
    {
        if (itr->second->object == object)
        {
            //Action Test
            //delete itr->second;
            m_SqMap.erase(itr);
            break;
        }
    }
}

void UISequencer::ClearAllObjects()
{
    for (pair<int, SQ_Group*>g : m_SqMap)
    {
        delete g.second;
    }
    m_SqMap.clear();
}

void UISequencer::SetEndFrame(float frame)
{
    m_endFrame = frame;
    m_currentFrame = 0;
    g_CurFrame = 0;
}