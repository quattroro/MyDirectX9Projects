#pragma once

#if _DF_SDFFont_New_
#include "glyph.h"

#define SDF_CASH_SIZE 512

struct OriTexData
{
	unsigned char* texdata;
	int				texheight;
	int				texwidth;
	int				texsize;

	//FastFile::FILE* fp;
};

struct OriGlyphsData
{
	OriTexData				texdata;
	map<long, Glyph>		glyphs;
	float					margin;
};

struct CashedGlyphsData
{
	LPDIRECT3DTEXTURE9 texture;
	unsigned char* cashedCheck;
	map<long, Glyph> glyphs;
	int texheight;
	int texwidth;
};

class SDFGenerator
{
public:
	SDFGenerator(LPDIRECT3DDEVICE9 device);
	~SDFGenerator();
	bool Init(int supfontsize);
	Glyph* GetCashedGlyph(int fonttype, long unicode);
	//bool LoadGlyph(int fonttype, long unicode);// sdf ���� �˰����� �̿��Ͽ� ���� ����
	bool LoadGlyph2(int fonttype, long unicode);// ���� unity font asset ���
	//bool LoadGlyph3(int fonttype, long unicode);// msdfgen ���̺귯���� �̿��Ͽ� sdf ����
	bool LoadGlyph4(int fonttype, long unicode);// freetype���� �����ϴ� sdf ����

	//void LoadFontAsseet(OriTexData& texdata, Json::Value jsonv, const char* filename);
	void LoadFontAsseet(OriTexData& texdata, const char* filename);
	LPDIRECT3DTEXTURE9 GetCashedTexture(int fonttype) { return m_CashedData[fonttype].texture; }
	float GetMargin(int fonttype);

	//void ResizeGlyph(int fonttype, Glyph* glyph);

private:
	float dist(int x0, int y0, int x1, int y1);
	bool isEdge(const unsigned char* bmp, int x, int y, int w, int h);
	void GenerateSDF(const unsigned char* bmp, int w, int h, float* sdf);
	void GenerateSDFAA(const unsigned char* bitmap, int width, int height, float* sdf);
	void NormalizeAndQuantizeWithSpread(const float* sdf, int size, float* out, float spread);
	void NormalizeSDF(float* sdf, int size);
	void NormalizeSDF3(float* sdf, int size);
	void QuantizeSDF(const float* sdf, int size, unsigned char* out);
	void GetSDF(OriTexData tex, int _x, int _y, int _width, int _height, unsigned char* sdf);

	void CasheSDFTextureRev(int index, const unsigned char* texdata, int width, int height, POINT* outpos, LPDIRECT3DTEXTURE9 debug = nullptr);
	void CasheSDFTexture(int index, const unsigned char* texdata, int width, int height, POINT* outpos, LPDIRECT3DTEXTURE9 debug = nullptr);
	bool ResizeSDFTextrue(int index);


	//void CasheDebugTex(lpdirect);
	POINT FindEmptyPos(const unsigned char* tex, int w, int h, int w2, int h2);
	POINT DeleteOldGlyph(const unsigned char* tex, int w, int h, int w2, int h2);
	bool IsEmptyRect(const unsigned char* tex, int width, int height, RECT rt);
	void ClearRect(unsigned char* tex, RECT rt);


private:
	//vector<LPDIRECT3DTEXTURE9>	m_CashedSDFTexture;			// ĳ���� SDF�ؽ���
	//vector<unsigned char*>		m_CashedCheck;
	//vector<map<long, Glyph>>	m_CashedGlyphs;


	vector<CashedGlyphsData>	m_CashedData;
	vector<OriGlyphsData>		m_OriginData;

	/*msdfgen::Shape mshape;
	msdfgen::FontMetrics mertics;*/

	LPDIRECT3DDEVICE9 m_device;
};
#endif