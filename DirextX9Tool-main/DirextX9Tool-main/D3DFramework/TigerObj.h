//#pragma once
//#include "GameObject.h"
//
//class TigerObj :public GameObject
//{
//public:
//	TigerObj(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 pos) :GameObject(pos) { }
//
//	TigerObj(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 pos, LPD3DXMESH Mesh, D3DMATERIAL9* g_pMeshMaterials);
//
//	TigerObj(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 pos, DWORD g_dwNumMaterialsArr, LPDIRECT3DTEXTURE9* g_pMeshTextures, LPD3DXMESH Mesh, D3DMATERIAL9* g_pMeshMaterials);
//
//	HRESULT InitMaterial(LPD3DXMESH Mesh, D3DMATERIAL9* g_pMeshMaterials);
//
//	void DirectionMove(D3DXVECTOR3 dir);
//
//	//void LookAt(D3DXVECTOR3 point);
//
//	void FowardMove(float speed);
//	void BackMove(float speed);
//	void RightMove(float speed);
//	void LeftMove(float speed);
//
//	void SetRotation();
//	//ȸ������ ���� ȸ����Ű�� ��ġ�� �̵�
//	void Render();
//
//private:
//	//����̽�
//	LPDIRECT3DDEVICE9 m_pd3dDevice;
//
//	//mesh
//	LPD3DXMESH m_TigerMesh;
//	D3DMATERIAL9* g_pMeshMaterials;
//	LPDIRECT3DTEXTURE9* g_pMeshTextures;
//	DWORD               g_dwNumMaterials;
//
//	float speed;
//	float rotspeed;
//
//	D3DXVECTOR3 MoveDir;
//	GameObject* childCamera;
//};
//
