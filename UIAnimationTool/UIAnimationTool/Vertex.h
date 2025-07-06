#pragma once

#define FVF_ ( D3DFVF_XYZ )
#define FVF_N ( D3DFVF_XYZ | D3DFVF_NORMAL)
#define FVF_NT ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)
#define FVF_NT2 ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX0 | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0))
#define FVF_NC ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE)
#define FVF_NCT ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1)
#define FVF_CT ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 )
#define FVF_C ( D3DFVF_XYZ | D3DFVF_DIFFUSE)
#define FVF_W ( D3DFVF_XYZRHW )
#define FVF_T ( D3DFVF_XYZ | D3DFVF_TEX1)
#define FVF_T1 ( D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE1(0) )
#define FVF_T2 ( D3DFVF_XYZ | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE1(1) )
#define FVF_TL ( D3DFVF_XYZ | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE2(1) )
#define FVF_WT ( D3DFVF_XYZRHW | D3DFVF_TEX1)
#define FVF_2D ( D3DFVF_LASTBETA_UBYTE4 | D3DFVF_TEX1)
#define FVF_W2D ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE )
#define FVF_WCT ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
#define FVF_BLEND ( D3DFVF_XYZB4)


struct VERTEX : public D3DXVECTOR3
{
	static const DWORD FVF;
};

struct VERTEX_N : public D3DXVECTOR3
{
	static const DWORD FVF;
	union
	{
		struct { D3DXVECTOR3 nv; };
		struct { float nx, ny, nz; };
	};

	//VERTEX_N::VERTEX_N(void) { }
};


struct VERTEX_NC : public D3DXVECTOR3
{
	static const DWORD FVF;
	union
	{
		struct { D3DXVECTOR3 nv; };
		struct { float nx, ny, nz; };
	};
	D3DCOLOR color;

};

struct VERTEX_NCT : public D3DXVECTOR3
{
	static const DWORD FVF;
	union
	{
		struct { D3DXVECTOR3 nv; };
		struct { float nx, ny, nz; };
	};
	D3DCOLOR color;
	float tu, tv;
};


struct VERTEX_CT : public D3DXVECTOR3
{
	static const DWORD FVF;
	D3DCOLOR color;
	float tu, tv;
};

struct VERTEX_C : public D3DXVECTOR3
{
	static const DWORD FVF;
	D3DCOLOR color;
};

struct VERTEX_W : public D3DXVECTOR3
{
	static const DWORD FVF;
	float rhw;
};

struct VERTEX_NT : public D3DXVECTOR3
{
	static const DWORD FVF;
	union
	{
		struct { D3DXVECTOR3 nv; };
		struct { FLOAT nx, ny, nz; };
	};
	FLOAT tu, tv;

	//VERTEX_NT::VERTEX_NT(void) { }
};

struct VERTEX_T : public D3DXVECTOR3
{
	static const DWORD FVF;
	FLOAT tu, tv;
};


struct VERTEX_T1 : public D3DXVECTOR3
{
	static const DWORD FVF;
	FLOAT tw;
};


struct VERTEX_T2 : public D3DXVECTOR3
{
	static const DWORD FVF;
	FLOAT tu, tv;
	FLOAT tw;
};

struct VERTEX_TL : public D3DXVECTOR3
{
	static const DWORD FVF;
	FLOAT tu, tv;
	FLOAT lu, lv;
};

struct VERTEX_WT : public D3DXVECTOR3
{
	static const DWORD FVF;
	FLOAT rhw;
	FLOAT tu, tv;
};

struct VERTEX_WCT : public D3DXVECTOR3
{
	static const DWORD FVF;
	FLOAT rhw;
	D3DCOLOR	color;
	FLOAT tu, tv;
};

struct VERTEX_2D : public D3DXVECTOR3
{
	static const DWORD FVF;
	DWORD dir;
	FLOAT tu, tv;
};

struct VERTEX_W2D : public D3DXVECTOR3
{
	static const DWORD FVF;
	FLOAT    rhw;
	D3DCOLOR color;

	VERTEX_W2D(FLOAT x1 = 0.0f, FLOAT y1 = 0.0f, FLOAT z1 = 0.0f, DWORD dwColor = 0x00ffffff)
	{
		x = x1;
		y = y1;
		z = z1;
		rhw = 1.0f;
		color = dwColor;
	}
};

D3DXMATRIX	MatrixInterpolate(D3DXMATRIX* prevMat, D3DXMATRIX* nextMat, float t);
float fabs_rad(float radian);

// Bias takes an X value between 0 and 1 and returns another value between 0 and 1
// The curve is biased towards 0 or 1 based on biasAmt, which is between 0 and 1.
// Lower values of biasAmt bias the curve towards 0 and higher values bias it towards 1.
//
// For example, with biasAmt = 0.2, the curve looks like this:
// 1
// |                                    *
// |                                  *
// |                                 *
// |                             **
// |                         **
// |                 ****
// |*********
// |___________________
// 0                   1
//
//
// With biasAmt = 0.8, the curve looks like this:
//
// 1
// |         **************
// |     **
// |   * 
// |  *
// | * 
// |* 
// |*  
// |___________________
// 0                   1
//
// With a biasAmt of 0.5, Bias returns X.
float Bias(float x, float biasAmt);


// Gain is similar to Bias, but biasAmt biases towards or away from 0.5.
// Lower bias values bias towards 0.5 and higher bias values bias away from it.
//
// For example, with biasAmt = 0.2, the curve looks like this:
//
// 1
// |                                      *
// |                                     *
// |                                 **
// |    ***************
// | **
// | *
// |*
// |___________________
// 0                   1
//
//
// With biasAmt = 0.8, the curve looks like this:
//
// 1
// |                       *****
// |                  ***
// |                *
// |               *
// |              *
// |         ***
// |*****
// |___________________
// 0                   1

float Gain(float x, float biasAmt);


// SmoothCurve maps a 0-1 value into another 0-1 value based on a cosine wave
// where the derivatives of the function at 0 and 1 (and 0.5) are 0. This is useful for
// any fadein/fadeout effect where it should start and end smoothly.
//
// The curve looks like this:
//
// 1
// |  		**
// | 	   *  *
// | 	  *	   *
// | 	  *	   *
// | 	 *		*
// |   **		 **
// |***			   ***
// |___________________
// 0                   1
//
float SmoothCurve(float x);
