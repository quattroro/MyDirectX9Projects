#include "pch.h"
//#include "TigerObj.h"
//#include "Utility.h"
//
//float DeltaTime;
//
//TigerObj::TigerObj(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 pos, LPD3DXMESH Mesh, D3DMATERIAL9* g_pMeshMaterials) :GameObject(pos)
//{
//    this->m_pd3dDevice = m_pd3dDevice;
//    //InitMaterial(Mesh, g_pMeshMaterials);
//}
//
//TigerObj::TigerObj(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 pos, DWORD g_dwNumMaterialsArr, LPDIRECT3DTEXTURE9* g_pMeshTextures, LPD3DXMESH Mesh, D3DMATERIAL9* g_pMeshMaterials)
//{
//    this->m_pd3dDevice = m_pd3dDevice;
//    this->pos = pos;
//    this->m_TigerMesh = Mesh;
//    this->g_pMeshMaterials = g_pMeshMaterials;
//    this->g_dwNumMaterials = g_dwNumMaterialsArr;
//    this->g_pMeshTextures = g_pMeshTextures;
//    speed = 10;
//}
//
//void TigerObj::FowardMove(float _speed)
//{
//	D3DXVECTOR3 temp = this->Axis[E_Axis_Forward] * speed * DeltaTime* _speed;
//	Translation(-temp.x, temp.y, -temp.z);
//}
//void TigerObj::BackMove(float _speed)
//{
//	D3DXVECTOR3 temp = this->Axis[E_Axis_Forward] * speed * DeltaTime * _speed;
//	Translation(-temp.x, temp.y, -temp.z);
//}
//void TigerObj::RightMove(float _speed)
//{
//	D3DXVECTOR3 temp = this->Axis[E_Axis_Right] * speed * DeltaTime * _speed;
//	Translation(-temp.x, temp.y, -temp.z);
//}
//void TigerObj::LeftMove(float _speed)
//{
//	D3DXVECTOR3 temp = this->Axis[E_Axis_Right] * speed * DeltaTime * _speed;
//	Translation(-temp.x, temp.y, -temp.z);
//}
//
//
//void TigerObj::DirectionMove(D3DXVECTOR3 dir)
//{
//    D3DXVECTOR3 temp;
//    MoveDir = dir;
//    if (getDegree(this->Axis[E_Axis_Forward], dir) >= 1 || getDegree(this->Axis[E_Axis_Forward], dir) <= -1)
//    {
//        temp = pos + dir;
//        LookAt(dir);
//    }
//
//    //this->pos += this->Axis[E_Axis_Forward] * speed;
//	temp = this->Axis[E_Axis_Forward] * speed * DeltaTime;
//    Translation(-temp.x, temp.y, -temp.z);
//
//    /*if (childCamera != nullptr)
//    {
//        childCamera->Translation(temp.x, temp.y, temp.z);
//    }*/
//
//}
//
//
//
//////앞 방향과 움직일 방향의 각을 구해서 회전시켜 준다.
////void TigerObj::LookAt(D3DXVECTOR3 point)
////{
////    D3DXVECTOR3 temp;
////    D3DXVec3Normalize(&temp, &point);
////
////    float angle = acos(D3DXVec3Dot(&Axis[E_Axis_Forward], &temp));
////
////    YawRotation(angle * 180.0f / 3.14f);
////}
//
//void TigerObj::SetRotation()
//{
//	D3DXMATRIXA16 matworld;
//	D3DXMATRIXA16 mattrans;
//	D3DXMATRIXA16 matrot;
//
//	D3DXMatrixIdentity(&matworld);
//
//	D3DXMatrixRotationX(&matrot, rot.x * 3.14 / 180);
//	D3DXMatrixMultiply(&matworld, &matworld, &matrot);
//
//	D3DXMatrixRotationY(&matrot, rot.y * 3.14 / 180);
//	D3DXMatrixMultiply(&matworld, &matworld, &matrot);
//
//	D3DXMatrixRotationZ(&matrot, rot.z * 3.14 / 180);
//	D3DXMatrixMultiply(&matworld, &matworld, &matrot);
//
//	D3DXMatrixTranslation(&mattrans, pos.x, pos.y, pos.z);
//	D3DXMatrixMultiply(&matworld, &matworld, &mattrans);
//
//	m_pd3dDevice->SetTransform(D3DTS_WORLD, &matworld);
//}
//
////회전값에 따라 회전시키고 위치로 이동
//void TigerObj::Render()
//{
//	D3DXMATRIXA16 worldmat = SetMatrix();
//	m_pd3dDevice->SetTransform(D3DTS_WORLD, &worldmat);
//
//	for (DWORD j = 0; j < g_dwNumMaterials; j++)
//	{
//		// Set the material and texture for this subset
//		m_pd3dDevice->SetMaterial(&g_pMeshMaterials[j]);
//		m_pd3dDevice->SetTexture(0, g_pMeshTextures[j]);
//
//		// Draw the mesh subset
//		m_TigerMesh->DrawSubset(j);
//	}
//	//m_TigerMesh->DrawSubset(0);
//
//}