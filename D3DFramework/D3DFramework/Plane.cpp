#include "Plane.h"


Plane::Plane(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 pos, DWORD g_dwNumMaterialsArr, LPDIRECT3DTEXTURE9* g_pMeshTextures, LPD3DXMESH Mesh, D3DMATERIAL9* g_pMeshMaterials)
{
	this->m_pd3dDevice = m_pd3dDevice;
	this->pos = pos;
	this->m_TigerMesh = Mesh;
	this->g_pMeshMaterials = g_pMeshMaterials;
	this->g_dwNumMaterials = g_dwNumMaterialsArr;
	this->g_pMeshTextures = g_pMeshTextures;
	
	Scaling(50, 10, 50);

}

void Plane::Render()
{
	D3DXMATRIXA16 worldmat = SetMatrix();
	m_pd3dDevice->SetTransform(D3DTS_WORLD, &worldmat);

	for (DWORD j = 0; j < g_dwNumMaterials; j++)
	{
		// Set the material and texture for this subset
		m_pd3dDevice->SetMaterial(&g_pMeshMaterials[j]);
		m_pd3dDevice->SetTexture(0, g_pMeshTextures[j]);

		// Draw the mesh subset
		m_TigerMesh->DrawSubset(j);
	}


}
