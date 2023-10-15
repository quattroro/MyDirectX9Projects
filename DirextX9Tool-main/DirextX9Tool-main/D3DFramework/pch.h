#pragma once
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "winmm.lib")


#include <Windows.h>
#include <mmsystem.h>
#include <d3dx9.h>
#include <d3d9.h>
#include <d3dx9shader.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <memory>
#include <math.h>

#include "Utility.h"

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_TEX1|D3DFVF_XYZ|D3DFVF_XYZ)


using namespace std;


using Vector4 = D3DXVECTOR4;
using Vector3 = D3DXVECTOR3;
using Vector2 = D3DXVECTOR2;
using Matrix = D3DXMATRIXA16;
using Quaternion = D3DXQUATERNION;

const float PI = (float)(3.141592741);

struct Vertex
{

	Vertex() {}
	Vertex(Vector3 p, Vector2 u, Vector3 n, Vector3 t)
	{
		pos = p;
		uv = u;
		normal = n;
		tangent = t;
	}


	Vector3 pos;
	Vector2 uv;
	Vector3 normal;
	Vector3 tangent;


};