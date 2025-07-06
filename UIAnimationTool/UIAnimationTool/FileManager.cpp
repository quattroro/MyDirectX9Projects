#include "stdafx.h"
#include "FileManager.h"
#include "Engine.h"
#include "UIManager.h"
#include "UIHierarkhy.h"
#include "UISequencer.h"
#include "GameObject.h"

FileManager* g_FileManager = new FileManager();

FileManager::FileManager()
{
}

FileManager::~FileManager()
{
}

Json::Value FileManager::ArrayToJson(float time, float* arr, int size, int bindex, D3DXVECTOR2 b1, D3DXVECTOR2 b2)
{
    Json::Value val;

    val["B1"]["x"] = b1.x;
    val["B1"]["y"] = b1.y;
    val["B2"]["x"] = b2.x;
    val["B2"]["y"] = b2.y;
    val["bezierindex"] = bindex;
    for (int i = 0; i < size; i++)
        val["value"].append(arr[i]);
    val["time"] = time;

    return val;
}

Json::Value FileManager::KeyToJson(vector<SQ_Key> _key)
{
    Json::Value keys;
    for (int i = 0; i < _key.size(); i++)
    {
        Json::Value key;
        key["position"] = ArrayToJson(_key[i].aniValue.Time, _key[i].aniValue.TransformValue.m_position, 2, _key[i].aniValue.bezierIndex, _key[i].aniValue.B1, _key[i].aniValue.B2);
        key["size"] = ArrayToJson(_key[i].aniValue.Time, _key[i].aniValue.TransformValue.m_size, 2, _key[i].aniValue.bezierIndex, _key[i].aniValue.B1, _key[i].aniValue.B2);
        key["scale"] = ArrayToJson(_key[i].aniValue.Time, _key[i].aniValue.TransformValue.m_scale, 2, _key[i].aniValue.bezierIndex, _key[i].aniValue.B1, _key[i].aniValue.B2);
        key["rotation"] = ArrayToJson(_key[i].aniValue.Time, &_key[i].aniValue.TransformValue.m_rotation, 1, _key[i].aniValue.bezierIndex, _key[i].aniValue.B1, _key[i].aniValue.B2);
        key["opacity"] = ArrayToJson(_key[i].aniValue.Time, &_key[i].aniValue.TransformValue.m_opacity, 1, _key[i].aniValue.bezierIndex, _key[i].aniValue.B1, _key[i].aniValue.B2);
        key["color"] = ArrayToJson(_key[i].aniValue.Time, _key[i].aniValue.TransformValue.m_color, 4, _key[i].aniValue.bezierIndex, _key[i].aniValue.B1, _key[i].aniValue.B2);
        keys.append(key);
    }
    return keys;
}

Json::Value FileManager::ObjectToJson(GameObject* _obj)
{
    Json::Value object;

    if (((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetGroup(_obj) != nullptr)
    {
        vector<SQ_Key> veckeys = ((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetGroup(_obj)->GetTimeLine(TIMELINE_TYPE::TL_GROUP)->GetKeys();
        Json::Value keys = KeyToJson(veckeys);
        object["keys"] = keys;
        object["objname"] = _obj->GetName().c_str();
        object["filename"] = _obj->GetFileName().c_str();
        object["filepath"] = _obj->GetFilePath().c_str();
    }
    return object;
}

bool FileManager::SaveFile(const char* filename)
{
    m_JsonData.clear();

    vector<TreeNode<GameObject*>*> nodes = ((UIHierarkhy*)g_UIManager->GetUI(UI_TYPE::HIERARKHY))->GetAllNodes();
    m_JsonData["name"] = nodes[0]->GetData()->GetName().c_str();
    m_JsonData["maxframe"] = ((UISequencer*)g_UIManager->GetUI(UI_TYPE::SEQUENCER))->GetEndFrame();

    //최상위 노드는 object정보에 넣지 않는다.
    for (int i = 1; i < nodes.size(); i++)
    {
        TreeNode<GameObject*>* node = nodes[i];
        Json::Value object = ObjectToJson(node->GetData());
        m_JsonData["objects"].append(object);
    }

    ofstream outstream;
    outstream.open(/*"./data/Animation.json"*/filename, ios::out);

    if (!outstream.is_open())
        return false;

    outstream << m_JsonData << endl;
    outstream.close();

    return true;
}

bool FileManager::SaveFile()
{
    OPENFILENAME OFN;
    TCHAR filePathName[100] = "";
    TCHAR lpstrFile[1024] = "";
    static TCHAR filter[] = "*.json";

    memset(&OFN, 0, sizeof(OPENFILENAME));
    OFN.lStructSize = sizeof(OPENFILENAME);
    OFN.hwndOwner = gEngine->GethWnd();
    OFN.lpstrFilter = filter;
    OFN.lpstrFile = lpstrFile;
    OFN.nMaxFile = 1024;
    OFN.lpstrInitialDir = ".\\data";
    OFN.lpstrDefExt = "json";
    //OFN.nMaxFileTitle = 100;
    //OFN.lpstrFileTitle = filename;

    char currentDir[1024];
    GetCurrentDirectory(1024, currentDir);

    if (GetSaveFileName(&OFN) != 0)
    {
        SetCurrentDirectory(currentDir);
        return SaveFile(lpstrFile);
    }
    return true;
}

void FileManager::LoadImageRes()
{

}

LPDIRECT3DTEXTURE9 FileManager::LoadTexture(DWORD* pWidth, DWORD* pHeight, char* filename, char* filepath)
{
    OPENFILENAME OFN;
    TCHAR filePathName[100] = "";
    TCHAR lpstrFile[1024] = "";
    static TCHAR filter[] = "모든 파일\0*.*\0텍스트 파일\0*.txt\0fbx 파일\0*.fbx";

    memset(&OFN, 0, sizeof(OPENFILENAME));
    OFN.lStructSize = sizeof(OPENFILENAME);
    OFN.hwndOwner = gEngine->GethWnd();
    OFN.lpstrFilter = filter;
    OFN.lpstrFile = lpstrFile;
    OFN.nMaxFile = 1024;
    OFN.lpstrInitialDir = ".\\Resources";
    OFN.nMaxFileTitle = 100;
    OFN.lpstrFileTitle = filename;



    /*strcpy(filename, "Resources/p_cs_item_name_01_00.dds");
    return CreateTextureFromDDS("Resources/p_cs_item_name_01_00.dds", pWidth, pHeight);*/

    char currentDir[1024];
    GetCurrentDirectory(1024, currentDir);

    if (GetOpenFileName(&OFN) != 0)
    {
        LPDIRECT3DTEXTURE9 tex;
        strcpy(filepath, OFN.lpstrFile);
        char* type[10] = { 0 };
        if (strstr(OFN.lpstrFile, ".dds") != NULL)
        {
            tex = CreateTextureFromDDS(OFN.lpstrFile, pWidth, pHeight);
            SetCurrentDirectory(currentDir);
            return tex;
        }
        else if (strstr(OFN.lpstrFile, ".png") != NULL)
        {
            tex = CreateTextureFromPNG(OFN.lpstrFile, pWidth, pHeight);
            SetCurrentDirectory(currentDir);
            return tex;
        }
    }

    SetCurrentDirectory(currentDir);

    //OFN.

    return nullptr;
}


LPDIRECT3DTEXTURE9 FileManager::LoadTexture(DWORD* pWidth, DWORD* pHeight, const char* filepath)
{
    if (strstr(filepath, ".dds") != NULL)
    {
        return CreateTextureFromDDS(filepath, pWidth, pHeight);
    }
    else if (strstr(filepath, ".png") != NULL)
    {
        return CreateTextureFromPNG(filepath, pWidth, pHeight);
    }
}

bool FileManager::LoadFile()
{
    OPENFILENAME OFN;
    TCHAR filePathName[100] = "";
    TCHAR lpstrFile[100] = "";
    static TCHAR filter[] = "모든 파일\0*.*\0텍스트 파일\0*.txt\0fbx 파일\0*.fbx";

    memset(&OFN, 0, sizeof(OPENFILENAME));
    OFN.lStructSize = sizeof(OPENFILENAME);
    OFN.hwndOwner = gEngine->GethWnd();
    OFN.lpstrFilter = filter;
    OFN.lpstrFile = lpstrFile;
    OFN.nMaxFile = 100;
    OFN.lpstrInitialDir = ".\\data";

    char currentDir[1024];
    GetCurrentDirectory(1024, currentDir);

    if (GetOpenFileName(&OFN) != 0)
    {
        SetCurrentDirectory(currentDir);
        return LoadFile(OFN.lpstrFile);
    }

    SetCurrentDirectory(currentDir);

    return false;
}

bool FileManager::LoadFile(const char* filename)
{
    m_JsonData.clear();

    Json::CharReaderBuilder builder;
    JSONCPP_STRING errs;

    char filepath[125] = { 0 };
    sprintf(filepath, "%s", filename);

    ifstream fin(filepath);

    if (!fin.is_open())
        return false;

    if (!parseFromStream(builder, fin, &m_JsonData, &errs))
    {
        std::cout << errs << std::endl;
        return false;
    }

    /*int count =  m_JsonData["objects"].size();
    for (int i = 0; i < count; i++)
    {
        string str = m_JsonData["objects"][i]["objname"].asCString();
    }*/

    fin.close();

    return true;
}

Json::Value& FileManager::GetLoadData()
{
    return m_JsonData;
}

LPDIRECT3DTEXTURE9 FileManager::CreateTextureFromDDS(const char* fileName, DWORD* pWidth, DWORD* pHeight)
{
    LPDIRECT3DTEXTURE9 pTexture = NULL;

    DWORD s_maxTextureWidth = gEngine->GetCaps().MaxTextureWidth;
    DWORD s_maxTextureHeight = gEngine->GetCaps().MaxTextureHeight;

    FILE* fp = fopen(fileName, "rb");

    if (fp == NULL)
    {
        //errorMsg << fileName << " Not Found! ";
        //Assert(0);
        return NULL;
    }

    BYTE DDSHeader[0x20];
    fread(&DDSHeader, sizeof(DDSHeader), 1, fp);
    DWORD dwFOURCC;
    memcpy(&dwFOURCC, DDSHeader, sizeof(DWORD));
    if (dwFOURCC != MAKEFOURCC('D', 'D', 'S', ' '))
    {
        //errorMsg << fileName << " DDS INVALID FORMAT!! ";
    }

    DWORD Width, Height, numSubLevel;
    memcpy(&Height, DDSHeader + 12, sizeof(DWORD));
    memcpy(&Width, DDSHeader + 16, sizeof(DWORD));

    DWORD scaledWidth = min(Width, s_maxTextureWidth);
    DWORD scaledHeight = min(Height, s_maxTextureHeight);
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    BYTE* buffer = new BYTE[fsize];

    if (buffer == NULL)
    {
        fclose(fp);
        return NULL;
    }

    fread(buffer, 1, fsize, fp);
    D3DXIMAGE_INFO ImgInfo;
    D3DXCreateTextureFromFileInMemoryEx(gEngine->GetDevice(), buffer, fsize, scaledWidth, scaledHeight, 0, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0, &ImgInfo, NULL, &pTexture);

    delete[] buffer;
    buffer = NULL;

    if (pWidth)*pWidth = ImgInfo.Width;
    if (pHeight)*pHeight = ImgInfo.Height;
    fclose(fp);

    return pTexture;
}


LPDIRECT3DTEXTURE9 FileManager::CreateTextureFromPNG(const char* fileName, DWORD* pWidth, DWORD* pHeight)
{
    LPDIRECT3DTEXTURE9 pTexture = NULL;
    static DWORD s_maxTextureWidth = gEngine->GetCaps().MaxTextureWidth;
    static DWORD s_maxTextureHeight = gEngine->GetCaps().MaxTextureHeight;

    FILE* fp = fopen(fileName, "rb");
    if (fp == NULL)
    {
        //errorMsg << fileName << " Not Found! ";
        //Assert(0);
        return NULL;
    }

    //8byte 버리고 4byte 뜯어냄 -> HeaderSize
    int HeaderSize = 0;
    fseek(fp, 8, SEEK_SET);
    fread((void*)&HeaderSize, 4, 1, fp);

    //PNG는 정수값이 빅엔디안 값으로 들어있기 때문에 리틀엔디안으로 변환해준다.
    HeaderSize = ((unsigned int)((((HeaderSize) & 0xff000000) >> 24) | (((HeaderSize) & 0x00ff0000) >> 8) | (((HeaderSize) & 0x0000ff00) << 8) | (((HeaderSize) & 0x000000ff) << 24)));


    //17byte 위치부터 HeaderSize 만큼 가져온다
    //해당 데이터의 앞 4byte ,4byte 가 width, height 값이다.
    char PNGHeader[125] = { 0 };
    fseek(fp, 16, SEEK_SET);
    fread(&PNGHeader, HeaderSize, 1, fp);

    int Width, Height, numSubLevel;
    memcpy(&Width, PNGHeader, sizeof(int));
    Width = ((unsigned int)((((Width) & 0xff000000) >> 24) | (((Width) & 0x00ff0000) >> 8) | (((Width) & 0x0000ff00) << 8) | (((Width) & 0x000000ff) << 24)));
    memcpy(&Height, PNGHeader + 4, sizeof(int));
    Height = ((unsigned int)((((Height) & 0xff000000) >> 24) | (((Height) & 0x00ff0000) >> 8) | (((Height) & 0x0000ff00) << 8) | (((Height) & 0x000000ff) << 24)));
    DWORD scaledWidth = min(Width, s_maxTextureWidth);
    DWORD scaledHeight = min(Height, s_maxTextureHeight);

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    //(E) 08.09.05
    BYTE* bbuffer = new BYTE[fsize];
    ZeroMemory(bbuffer, sizeof(BYTE) * (fsize));
    fread(bbuffer, 1, fsize, fp);

    D3DXIMAGE_INFO ImgInfo;

    HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(gEngine->GetDevice(), bbuffer, fsize, scaledWidth, scaledHeight, 0, 0, D3DFMT_UNKNOWN/*D3DFMT_A8R8G8B8*/, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0, &ImgInfo, NULL, &pTexture);

    delete[] bbuffer;
    bbuffer = NULL;

    if (pWidth)*pWidth = ImgInfo.Width;
    if (pHeight)*pHeight = ImgInfo.Height;
    fclose(fp);
    return pTexture;
}