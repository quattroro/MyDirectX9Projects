#pragma once
class GameObject;
class Transform;

class FileManager
{
public:
	FileManager();
	~FileManager();


	bool SaveFile(const char* filename);
	bool SaveFile();
	bool LoadFile(const char* filename);
	bool LoadFile();

	LPDIRECT3DTEXTURE9 LoadTexture(DWORD* pWidth, DWORD* pHeight, char* filename, char* filepath);
	LPDIRECT3DTEXTURE9 LoadTexture(DWORD* pWidth, DWORD* pHeight, const char* filepath);

	void LoadImageRes();
	LPDIRECT3DTEXTURE9 CreateTextureFromDDS(const char* fileName, DWORD* pWidth, DWORD* pHeight);
	LPDIRECT3DTEXTURE9 CreateTextureFromPNG(const char* fileName, DWORD* pWidth, DWORD* pHeight);

	Json::Value& GetLoadData();

private:
	Json::Value ObjectToJson(GameObject* _obj);
	Json::Value KeyToJson(vector<SQ_Key> _key);
	Json::Value ArrayToJson(float time, float* arr, int size, int bindex, D3DXVECTOR2 b1, D3DXVECTOR2 b2);

private:
	Json::Value m_JsonData;
};

extern FileManager* g_FileManager;
