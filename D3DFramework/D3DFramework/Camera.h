#pragma once
#include "GameObject.h"


class Camera: public GameObject
{
public:
	Camera(LPDIRECT3DDEVICE9 m_pd3dDevice);

	Camera(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 vEyePt, D3DXVECTOR3 vLookatPt, D3DXVECTOR3 vUpVec);


	void Init(D3DXVECTOR3 vEyePt, D3DXVECTOR3 vLookatPt, D3DXVECTOR3 vUpVec);

	//À§Ä¡¿Í lookatÀÌ °°ÀÌ ¿òÁ÷¿©¾ß ÇÑ´Ù.
	//virtual void Translation(float x, float y, float z);

	////²ô´ö²ô´ö
	//virtual void PitchRotation(float fAngle);
	////µµ¸®µµ¸®
	//virtual void YawRotation(float fAngle);
	

	//µµ¸®µµ¸®
	//void RotHor(float ydegree);

	//²ô´ö²ô´ö
	//void RotVir(float xdegree);


	//²ô´ö²ô´ö
	virtual void PitchRotation(float fAngle);
	//µµ¸®µµ¸®
	virtual void YawRotation(float fAngle);
	//°¼¿ì¶×
	virtual void RollRotation(float fAngle);

	//À§Ä¡¿Í lookatÀÌ °°ÀÌ ¿òÁ÷¿©¾ß ÇÑ´Ù.
	/*void Translation(float x, float y, float z)
	{
		direction = D3DXVECTOR3(x, y, z);
		vEyePt += direction;
		vLookatPt += direction;
	}*/

	void RightMove();

	void LeftMove();

	void FowardMove();

	void BackMove();


	void Render();

	//ÇØ´ç ¹æÇâÀ» ¹Ù¶óº¸µµ·Ï 
	void LookAt(D3DXVECTOR3 pos);

	void SetMoveMode(int mode);

private:
	LPDIRECT3DDEVICE9 m_pd3dDevice;

	D3DXVECTOR3 vEyePt;
	D3DXVECTOR3 vLookatPt;
	D3DXVECTOR3 vUpVec;

	D3DXVECTOR3 direction;

	float LookDistance = 10;

	float movespeed = 10.0f;
	float rotspeed = 0.4f;


	D3DXVECTOR3 First_vEyePt;
	D3DXVECTOR3 First_vLookatPt;
	D3DXVECTOR3 First_vUpVec;

	int movemode = 0;

};

