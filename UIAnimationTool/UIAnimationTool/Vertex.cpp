#include "StdAfx.h"
#include "Vertex.h"

D3DXMATRIX MatrixInterpolate(D3DXMATRIX* prevMat, D3DXMATRIX* nextMat, float t)
{
	D3DXMATRIX mat;
	static D3DXQUATERNION prevQ, nextQ, q;
	D3DXQuaternionRotationMatrix(&prevQ, prevMat);
	D3DXQuaternionRotationMatrix(&nextQ, nextMat);
	D3DXQuaternionSlerp(&q, &prevQ, &nextQ, t);
	D3DXMatrixRotationQuaternion(&mat, &q);

	mat._41 = prevMat->_41 + (nextMat->_41 - prevMat->_41) * t;
	mat._42 = prevMat->_42 + (nextMat->_42 - prevMat->_42) * t;
	mat._43 = prevMat->_43 + (nextMat->_43 - prevMat->_43) * t;
	return mat;
}

float Bias(float x, float biasAmt)
{
	// WARNING: not thread safe
	static float lastAmt = -1;
	static float lastExponent = 0;
	if (lastAmt != biasAmt)
	{
		lastExponent = log(biasAmt) * -1.4427f; // (-1.4427 = 1 / log(0.5))
	}
	return pow(x, lastExponent);
}

float Gain(float x, float biasAmt)
{
	// WARNING: not thread safe
	if (x < 0.5)
		return 0.5f * Bias(2 * x, 1 - biasAmt);
	else
		return 1 - 0.5f * Bias(2 - 2 * x, 1 - biasAmt);
}

float SmoothCurve(float x)
{
	return (1 - cos(x * D3DX_PI)) * 0.5f;
}

float fabs_rad(float radian)
{
	if (radian < 0.0f)
	{
		float minusRad = -radian;
		int nNum = minusRad / (2 * D3DX_PI);
		minusRad -= nNum * (2 * D3DX_PI);
		return minusRad;
	}
	else
	{
		int nNum = radian / (2 * D3DX_PI);
		radian -= nNum * (2 * D3DX_PI);
		return radian;
	}
}

const DWORD VERTEX::FVF = D3DFVF_XYZ;
const DWORD VERTEX_N::FVF = D3DFVF_XYZ | D3DFVF_NORMAL;
const DWORD VERTEX_NC::FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE;
const DWORD VERTEX_NCT::FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;
const DWORD VERTEX_CT::FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
const DWORD VERTEX_C::FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE;
const DWORD VERTEX_W::FVF = D3DFVF_XYZRHW;
const DWORD	VERTEX_NT::FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;
const DWORD	VERTEX_T::FVF = D3DFVF_XYZ | D3DFVF_TEX1;
const DWORD VERTEX_T1::FVF = D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE1(0);
const DWORD	VERTEX_T2::FVF = D3DFVF_XYZ | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE1(1);
const DWORD	VERTEX_TL::FVF = D3DFVF_XYZ | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE2(1);
const DWORD	VERTEX_WT::FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;
const DWORD VERTEX_WCT::FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
const DWORD	VERTEX_2D::FVF = D3DFVF_LASTBETA_UBYTE4 | D3DFVF_TEX1;
const DWORD	VERTEX_W2D::FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;