#pragma once
#include "GameObject.h"

enum class PROJECTION_TYPE
{
	PERSPECTIVE,
	ORTHOGRAPHIC,
};


class Camera: public Component
{
public:
	Camera();

	virtual ~Camera();

	virtual void FinalUpdate() override;

	void Render();

	//void Init(D3DXVECTOR3 vEyePt, D3DXVECTOR3 vLookatPt, D3DXVECTOR3 vUpVec);

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


	////��������
	//virtual void PitchRotation(float fAngle);
	////��������
	//virtual void YawRotation(float fAngle);
	////�����
	//virtual void RollRotation(float fAngle);

	//��ġ�� lookat�� ���� �������� �Ѵ�.
	/*void Translation(float x, float y, float z)
	{
		direction = D3DXVECTOR3(x, y, z);
		vEyePt += direction;
		vLookatPt += direction;
	}*/

	//void RightMove();

	//void LeftMove();

	//void FowardMove();

	//void BackMove();


	//void Render();

	////�ش� ������ �ٶ󺸵��� 
	//void LookAt(D3DXVECTOR3 pos);

	//void SetMoveMode(int mode);

	Matrix& GetViewMatrix() { return _matView; }
	Matrix& GetProjectionMatrix() { return _matProjection; }

private:
	//�⺻�� ��������
	PROJECTION_TYPE _type = PROJECTION_TYPE::PERSPECTIVE;

	float _near = 1.0f;
	float _far = 1000.0f;
	float _fov = PI / 4.0f;
	float _scale = 1.0f;

	Matrix _matView = {};
	Matrix _matProjection = {};

	D3DXVECTOR3 vEyePt;
	D3DXVECTOR3 vLookatPt;
	D3DXVECTOR3 vUpVec;
//public:
//	// TEMP
//	static Matrix S_MatView;
//	static Matrix S_MatProjection;
};

