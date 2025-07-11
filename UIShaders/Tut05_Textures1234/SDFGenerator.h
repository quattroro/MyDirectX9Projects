#pragma once
#include "glyph.h"

#define SDF_CASH_SIZE 2048

class SDFGenerator
{
public:
	SDFGenerator(LPDIRECT3DDEVICE9 device);
	bool Init();
	Glyph GetCashedGlyph(long unicode);
	void LoadGlyph(long unicode);

	LPDIRECT3DTEXTURE9 GetCashedTexture() { return m_CashedSDFTexture; }

private:
	float dist(int x0, int y0, int x1, int y1);
	bool isEdge(const unsigned char* bmp, int x, int y, int w, int h);
	void GenerateSDF(const unsigned char* bmp, int w, int h, float* sdf);
	void NormalizeAndQuantizeWithSpread(const float* sdf, int size, float* out, float spread);
	void NormalizeSDF(float* sdf, int size);
	void QuantizeSDF(const float* sdf, int size, unsigned char* out);

	void CasheSDFTexture(const unsigned char* texdata, int width, int height);
	POINT FindEmptyPos(const unsigned char* tex, int w, int h, int w2, int h2);
	bool IsEmptyRect(const unsigned char* tex, int width, int height, RECT rt);
	void ClearRect(unsigned char* tex, RECT rt);

private:
	int							m_Padding;
	int							m_Size;

	LPDIRECT3DDEVICE9			m_Device;
	LPDIRECT3DTEXTURE9			m_CashedSDFTexture;			// 캐쉬된 SDF텍스쳐
	unsigned char*				m_CashedCheck;
	std::map<long, Glyph>		m_CashedGlyphs;
};