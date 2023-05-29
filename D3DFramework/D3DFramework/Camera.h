#pragma once
#include "GameObject.h"


class Camera: public GameObject
{
public:
	Camera(LPDIRECT3DDEVICE9 m_pd3dDevice);

	Camera(LPDIRECT3DDEVICE9 m_pd3dDevice, D3DXVECTOR3 vEyePt, D3DXVECTOR3 vLookatPt, D3DXVECTOR3 vUpVec);


	void Init(D3DXVECTOR3 vEyePt, D3DXVECTOR3 vLookatPt, D3DXVECTOR3 vUpVec);

	//��ġ�� lookat�� ���� �������� �Ѵ�.
	//virtual void Translation(float x, float y, float z);

	////��������
	//virtual void PitchRotation(float fAngle);
	////��������
	//virtual void YawRotation(float fAngle);
	

	//��������
	//void RotHor(float ydegree);

	//��������
	//void RotVir(float xdegree);


	//��������
	virtual void PitchRotation(float fAngle);
	//��������
	virtual void YawRotation(float fAngle);
	//�����
	virtual void RollRotation(float fAngle);

	//��ġ�� lookat�� ���� �������� �Ѵ�.
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

	//�ش� ������ �ٶ󺸵��� 
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

