#pragma once

class Transform;
//2d용 게임 오브젝트
class GameObject
{
private:
	static int _nextID;

public:
	GameObject();
	GameObject(const char* name, const char* filename = "");
	GameObject(const GameObject* object);
	~GameObject();

	bool operator== (const GameObject& other);
	bool operator== (const int InstanceID);

	void Render();
	//void SetTexture(const char* filename);
	void SetTexture(LPDIRECT3DTEXTURE9 tex, int width, int height, const char* filename, const char* filepath);
	void ReleaseTexture();
	void MakeVertexBuffer2D();

	void SetViewport();
	void GetViewport();

	string GetName() { return name; }
	string GetFileName() { return filename; }
	string GetFilePath() { return filepath; }
	void SetName(string name) { this->name = name; }
	int GetInstanceID() { return m_InstanceID; }
	Transform* GetTransform() { return m_transform; }
	void SetSelected(bool v) { m_Selected = v; }
	bool GetSelected() { return m_Selected; }
	RECT GetRect();
	D3DXVECTOR3* GetCorner();
	DWORD GetTexWidth() { return width; }
	DWORD GetTexHeight() { return height; }

	bool Intersects(D3DXVECTOR2 pt);

private:
	string name;
	string filename;
	string filepath;

	Transform* m_transform;
	LPDIRECT3DTEXTURE9 tex;
	DWORD width;
	DWORD height;

	RECT m_Rect;
	LPDIRECT3DVERTEXBUFFER9 m_pVB2D;
	int m_InstanceID;
	bool m_Selected;

	D3DXMATRIX m_matWorld;
	D3DXMATRIX m_matWVP;
	D3DXMATRIX m_matVP;

	ID3DXLine* m_Line;
	D3DXVECTOR3 m_Rect2[4];
};
