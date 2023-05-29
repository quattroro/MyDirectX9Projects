#pragma once
#include "GameObject.h"
class Plane:public GameObject
{
public:
	Plane(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 pos, DWORD g_dwNumMaterialsArr, LPDIRECT3DTEXTURE9* g_pMeshTextures, LPD3DXMESH Mesh, D3DMATERIAL9* g_pMeshMaterials);

	// GameObject��(��) ���� ��ӵ�
	virtual void Render() override;


private:
	//����̽�
	LPDIRECT3DDEVICE9 m_pd3dDevice;

	//mesh
	LPD3DXMESH m_TigerMesh;
	D3DMATERIAL9* g_pMeshMaterials;
	LPDIRECT3DTEXTURE9* g_pMeshTextures;
	DWORD               g_dwNumMaterials;


};

