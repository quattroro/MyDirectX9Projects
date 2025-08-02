#include "stdafx.h"
#include "SDFGenerator.h"
//#include "Device.h"
#include "metrics_parser.h"


#if _DF_SDFFont_New_

//#include <msdfgen.h>
//#include <msdfgen-ext.h>
//#pragma comment(lib, "msdfgen/msdfgen-core.lib")
//#pragma comment(lib, "msdfgen/msdfgen-ext.lib")
//
//#include "msdfgen/msdfgen.h"
//#include "msdfgen/msdfgen-ext.h"

//#include "FastFile.h"
//#include "SDFFontRenderer.h"


#endif

#if _DF_SDFFont_New_
#include FT_FREETYPE_H

string Support_Font_FileName[] =
{
	"gulim", "gulimche", "malgun", "dotum", "dotumche", "ngulim", "tahoma"
};

SDFGenerator::SDFGenerator(LPDIRECT3DDEVICE9 device)
{
	m_device = device;
}

bool SDFGenerator::Init(int supfontsize)
{
	m_CashedData.resize(supfontsize);
	for (int i = 0; i < supfontsize; i++)
	{
		if (FAILED(D3DXCreateTexture(m_device, SDF_CASH_SIZE, SDF_CASH_SIZE, 1, 0, D3DFORMAT::D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_CashedData[i].texture)))
		{
			return false;
		}
		m_CashedData[i].texwidth = SDF_CASH_SIZE;
		m_CashedData[i].texheight = SDF_CASH_SIZE;

		m_CashedData[i].cashedCheck = new unsigned char[SDF_CASH_SIZE * SDF_CASH_SIZE];
		ZeroMemory(m_CashedData[i].cashedCheck, SDF_CASH_SIZE * SDF_CASH_SIZE);
	}

	//for (int i = 0; i < supfontsize; i++)
	//{
	//	OriGlyphsData glyphsdata;
	//	float margin = 0;
	//	SDFont::MetricsParser parser(glyphsdata.glyphs, glyphsdata.margin);
	//	parser.parseSpec(forming("SDFFont\\SDF_MAT(%s).txt", Support_Font_FileName[i].c_str()));
	//	//LoadFontAsseet(glyphsdata.texdata, forming("SDFFont\\SDF_PNG2(%s).txt", Support_Font_FileName[i].c_str()));

	//	m_OriginData.push_back(glyphsdata);
	//}

	return true;
}

SDFGenerator::~SDFGenerator()
{
	for (int i = 0; i < 7; i++)
	{
		m_CashedData[i].texture->Release();
		delete[] m_CashedData[i].cashedCheck;
	}
}

float SDFGenerator::GetMargin(int fonttype)
{
	return m_OriginData[fonttype].margin / (float)m_CashedData[fonttype].texwidth;
}

//void SDFGenerator::LoadFontAsseet(OriTexData& texdata, Json::Value jsonv, const char* filename)
//{
//	char FileName[512] = { 0 };
//	strcpy(FileName, filename);
//	FILE* fp = fopen(FileName, "rb");
//
//	fseek(fp, 0, SEEK_END);
//	long fsize = ftell(fp);
//	fseek(fp, 0, SEEK_SET);
//
//	char* buffer = new char[fsize];
//	fread(buffer, 1, fsize, fp);
//
//	Json::CharReaderBuilder builder;
//	Json::CharReader* reader = builder.newCharReader();
//	Json::Value root;
//	string errors;
//
//	bool result2 = reader->parse(buffer, buffer + fsize, &root, &errors);
//
//	string name = root["MonoBehaviour"]["m_Name"].asCString();
//	string version = root["MonoBehaviour"]["m_Version"].asCString();
//
//	/*YAML::Node node = YAML::Load(buffer);
//
//	int a = node.size();
//
//	string name = node["MonoBehaviour"]["m_Name"].as<std::string>();
//	string version = node["MonoBehaviour"]["m_Version"].as<std::string>();*/
//
//
//	int width = root["Texture2D"]["m_Width"].as<int>() * 2;
//	int height = root["Texture2D"]["m_Height"].as<int>();
//	int texsize = root["Texture2D"]["image data"].as<int>() * 2;
//
//	const char* TexData = root["Texture2D"]["_typelessdata"].asCString();
//
//
//	//texdata.texdata = new unsigned char[texsize];
//	texdata.texheight = height;
//	texdata.texwidth = width;
//	texdata.texsize = texsize;
//
//	//memcpy(texdata.texdata, TexData, texsize);
//}

void SDFGenerator::LoadFontAsseet(OriTexData& texdata, const char* filename)
{
	char FileName[512] = { 0 };
	strcpy(FileName, filename);
	FILE* fp = fopen(FileName, "rb");

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char* buffer = new char[fsize];
	fread(buffer, 1, fsize, fp);

	/*int width = 4096 * 2;
	int height = 4096;
	int texsize = 4096 * 4096 * 2;*/

	int width = 4096;
	int height = 4096;
	int texsize = width * height;

	//texdata.fp = fp;
	texdata.texdata = new unsigned char[texsize];
	texdata.texheight = height;
	texdata.texwidth = width;
	texdata.texsize = texsize;

	memcpy(texdata.texdata, buffer, texsize);

	delete[] buffer;
}

Glyph* SDFGenerator::GetCashedGlyph(int fonttype, long unicode)
{
	Glyph* pGlyph = nullptr;

	//if (m_OriginData[fonttype].glyphs.count(unicode))
	{
		if (m_CashedData[fonttype].glyphs.count(unicode))
		{
			pGlyph = &m_CashedData[fonttype].glyphs[unicode];
		}
		else
		{
			//LoadGlyph(fonttype, unicode);
			//LoadGlyph2(fonttype, unicode);
			//LoadGlyph3(fonttype, unicode);
			LoadGlyph4(fonttype, unicode);
			pGlyph = &m_CashedData[fonttype].glyphs[unicode];
		}
	}

	return pGlyph;
}

bool SDFGenerator::LoadGlyph2(int fonttype, long unicode)
{
	Glyph glyph;

	memcpy(&glyph, &m_OriginData[fonttype].glyphs[unicode], sizeof(Glyph));

	int x = glyph.mTextureCoordX;
	int y = glyph.mTextureCoordY;
	int width = glyph.mTextureWidth;
	int height = glyph.mTextureHeight;

	if (x > 0)
		x = x - /*4*/m_OriginData[fonttype].margin;
	if (y > 0)
		y = y - /*4*/m_OriginData[fonttype].margin;
	if (width > 0)
		width = width + /*8*/m_OriginData[fonttype].margin * 2;
	if (height > 0)
		height = height + /*8*/m_OriginData[fonttype].margin * 2;

	std::vector<unsigned char> sdf8(width * height);
	GetSDF(m_OriginData[fonttype].texdata, x, y, width, height, sdf8.data());

	POINT pos;
	CasheSDFTexture(fonttype, sdf8.data(), width, height, &pos);


	glyph.mCodePoint = unicode;
	glyph.mTextureCoordX = (float)(pos.x) / (float)m_CashedData[fonttype].texwidth;
	glyph.mTextureCoordY = (float)(pos.y) / (float)m_CashedData[fonttype].texheight;
	glyph.mTextureWidth = (float)(width) / (float)m_CashedData[fonttype].texwidth;
	glyph.mTextureHeight = (float)(height) / (float)m_CashedData[fonttype].texheight;

	/*glyph.size = width * height;
	glyph.time = clock();*/

	//ResizeGlyph(fonttype, &glyph);


	m_CashedData[fonttype].glyphs.insert(pair<long, Glyph>(unicode, glyph));

	return true;
}

void SDFGenerator::GetSDF(OriTexData tex, int _x, int _y, int _width, int _height, unsigned char* sdf)
{
	for (int y = 0; y < _height; y++)
	{
		for (int x = 0; x < _width; x++)
		{
			char dec = 0;
			memcpy(&dec, &tex.texdata[((_y + y) * tex.texwidth) + (_x + x)], sizeof(char));
			sdf[y * _width + x] = dec;
		}
	}
}

bool SDFGenerator::LoadGlyph4(int fonttype, long unicode)
{
	int size = 35/*25*/;
	char fontpath[256] = { 0 };
	FT_Error error;

	FT_Library ft;
	FT_Face face;
	FT_Init_FreeType(&ft);

	if (fonttype == 0)
	{
		error = FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 0, &face);
	}
	else if (fonttype == 1)
	{
		error = FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 1, &face);
	}
	else if (fonttype == 2)
	{
		error = FT_New_Face(ft, "C:\\Windows\\Fonts\\malgun.ttf", 0, &face);
	}
	else if (fonttype == 3)
	{
		error = FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 2, &face);
	}
	else if (fonttype == 4)
	{
		error = FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 3, &face);
	}
	else if (fonttype == 5)
	{
		error = FT_New_Face(ft, "C:\\Windows\\Fonts\\NGULIM.TTF", 0, &face);
	}
	else if (fonttype == 6)
	{
		error = FT_New_Face(ft, "C:\\Windows\\Fonts\\tahoma.ttf", 0, &face);
	}

	if (error)
		return false;

	//if (/*FT_HAS_FIXED_SIZES(face)*/fonttype == 2)
	//{
	//	FT_Bitmap_Size* sizes = face->available_sizes;
	//	int count = face->num_fixed_sizes;

	//	for (int i = 1; i < count; i++)
	//	{
	//		if (sizes[i].height >= size)
	//		{
	//			size = sizes[i - 1].height;
	//		}
	//	}
	//}

	if (FT_Set_Pixel_Sizes(face, size, size))
		return false;

	if (FT_Load_Char(face, unicode, FT_LOAD_RENDER))
		return false;
	if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF))
		return false;

	FT_Bitmap& bitmap = face->glyph->bitmap;
	int width = bitmap.width;
	int height = bitmap.rows;

	/*int Padding = 3;
	width = width + Padding * 2;
	height = height + Padding * 2;*/

	//FT_Render_Glyph()

	float* sdf = new float[width * height];

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			sdf[y * width + x] = bitmap.buffer[y * width + x];
		}
	}

	//getFontMetrics(FontMetrics &metrics, FontHandle *font, FontCoordinateScaling coordinateScaling = FONT_SCALING_LEGACY);

	//NormalizeAndQuantizeWithSpread(sdf, width * height, sdf, /*16*/3/*5*/);

	NormalizeSDF3(sdf, width * height);

	std::vector<unsigned char> sdf8(width * height);
	QuantizeSDF(sdf, width * height, sdf8.data());

	POINT pos;
	CasheSDFTextureRev(fonttype, sdf8.data(), width, height, &pos);


	Glyph glyph;
	glyph.mCodePoint = unicode;
	glyph.mTextureCoordX = (float)(pos.x /*+ Padding*/) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texwidth;
	glyph.mTextureCoordY = (float)(pos.y /*+ Padding*/) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texheight;
	glyph.mTextureWidth = (float)(width /*- Padding * 2*/) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texwidth;
	glyph.mTextureHeight = (float)(height /*- Padding * 2*/) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texheight;
	glyph.mWidth = face->glyph->metrics.width / 64.0;
	glyph.mHeight = face->glyph->metrics.height / 64.0;
	glyph.mHorizontalBearingX = face->glyph->metrics.horiBearingX / 64.0;
	glyph.mHorizontalBearingY = face->glyph->metrics.horiBearingY / 64.0;
	glyph.mHorizontalAdvance = face->glyph->metrics.horiAdvance / 64.0;
	glyph.mVerticalBearingX = face->glyph->metrics.vertBearingX / 64.0;
	glyph.mVerticalBearingY = face->glyph->metrics.vertBearingY / 64.0;
	glyph.mVerticalAdvance = face->glyph->metrics.vertAdvance / 64.0;
	/*glyph.size = size;
	glyph.time = timeFunc();*/

	//ResizeGlyph(fonttype, &glyph);

	FT_Done_FreeType(ft);
	m_CashedData[fonttype].glyphs.insert(pair<long, Glyph>(unicode, glyph));
}
//bool SDFGenerator::LoadGlyph3(int fonttype, long unicode)
//{
//	int size = 30;
//	char fontpath[256] = { 0 };
//
//	FT_Library ft;
//	FT_Face face;
//	FT_Init_FreeType(&ft);
//
//	if (fonttype == 0)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 0, &face);
//	}
//	else if (fonttype == 1)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 1, &face);
//	}
//	else if (fonttype == 2)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\malgun.ttf", 0, &face);
//	}
//	else if (fonttype == 3)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 2, &face);
//	}
//	else if (fonttype == 4)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 3, &face);
//	}
//	else if (fonttype == 5)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\NGULIM.TTF", 0, &face);
//	}
//	else if (fonttype == 6)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\tahoma.ttf", 0, &face);
//	}
//
//	FT_Set_Pixel_Sizes(face, 0, size);
//	FT_Load_Char(face, unicode, FT_LOAD_RENDER);
//
//	FT_Bitmap& bitmap = face->glyph->bitmap;
//	int width = bitmap.width;
//	int height = bitmap.rows;
//
//	/*int Padding = 3;
//	width = width + Padding * 2;
//	height = height + Padding * 2;*/
//
//	//FT_Render_Glyph()
//
//	float* sdf;
//	msdfgen::FreetypeHandle* mft;
//	msdfgen::FontHandle* font;
//	msdfgen::Bitmap<float, 1> bsdf(width, height);
//	// 변환: 스케일 및 위치 조정 // scale, transrate, distancemapping
//	//msdfgen::SDFTransformation t(msdfgen::Projection(msdfgen::Vector2(bitmap.width, bitmap.rows), msdfgen::Vector2(0.125, 0.125)), msdfgen::Range(/*0.125*/0.125));
//
//
//	if (mft = msdfgen::initializeFreetype())
//	{
//		if (font = loadFont(mft, "C:\\Windows\\Fonts\\malgun.ttf"))
//		{
//			if (loadGlyph(mshape, font, unicode, msdfgen::FONT_SCALING_EM_NORMALIZED))
//			{
//				mshape.normalize();
//
//				//double left, bottom, right, top;
//				//mshape.bound(left, bottom, right, top);
//
//				//double glyphWidth = right - left, glyphHeight = top - bottom;
//				//double margin = 0.125; // 예: 4.0 (픽셀)
//				//double imageWidth = width, imageHeight = height;
//
//				//// Scale계산
//				//double scaleX = (imageWidth - 2.0 * margin) / glyphWidth;
//				//double scaleY = (imageHeight - 2.0 * margin) / glyphHeight;
//				//double scale = min(scaleX, scaleY);
//
//				//// Translate계산
//				//msdfgen::Vector2 translate(margin - left * scaleX, margin - bottom * scaleY);
//
//				msdfgen::SDFTransformation t(msdfgen::Projection(msdfgen::Vector2(bitmap.width - 0.001 * 2, bitmap.rows - 0.001 * 2), msdfgen::Vector2(0.001, 0.001)), msdfgen::Range(0.001));
//
//				msdfgen::generateSDF(bsdf, mshape, t);
//				sdf = new float[width * height];
//
//				for (int y = 0; y < height; y++)
//				{
//					for (int x = 0; x < width; x++)
//					{
//						sdf[y * width + x] = *bsdf(x, y);
//					}
//				}
//			}
//		}
//	}
//
//	//getFontMetrics(FontMetrics &metrics, FontHandle *font, FontCoordinateScaling coordinateScaling = FONT_SCALING_LEGACY);
//	//NormalizeAndQuantizeWithSpread(sdf, width * height, sdf, /*16*/3/*5*/);
//
//	NormalizeSDF3(sdf, width * height);
//
//	std::vector<unsigned char> sdf8(width * height);
//	QuantizeSDF(sdf, width * height, sdf8.data());
//
//	//int Padding = /*16*//*6*/10/*8*//*4*/;
//	//int newWidth = width + Padding * 2;
//	//int newHeight = height + Padding * 2;
//	//unsigned char* buffer2 = new unsigned char[newWidth * newHeight];
//	//memset(buffer2, 0, newWidth * newHeight);
//
//	//for (int y = 0; y < height; ++y)
//	//{
//	//	for (int x = 0; x < width; ++x)
//	//	{
//	//		buffer2[(y + Padding) * newWidth + (x + Padding)] = sdf8[y * width + x];
//	//	}
//	//}
//	//width = newWidth;
//	//height = newHeight;
//
//	POINT pos;
//	CasheSDFTexture(fonttype, sdf8.data(), width, height, &pos);
//
//
//	Glyph glyph;
//	glyph.mCodePoint = unicode;
//	glyph.mTextureCoordX = (float)(pos.x /*+ Padding*/) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texwidth;
//	glyph.mTextureCoordY = (float)(pos.y /*+ Padding*/) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texheight;
//	glyph.mTextureWidth = (float)(width /*- Padding * 2*/) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texwidth;
//	glyph.mTextureHeight = (float)(height /*- Padding * 2*/) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texheight;
//	glyph.mWidth = face->glyph->metrics.width / 64.0;
//	glyph.mHeight = face->glyph->metrics.height / 64.0;
//	glyph.mHorizontalBearingX = face->glyph->metrics.horiBearingX / 64.0;
//	glyph.mHorizontalBearingY = face->glyph->metrics.horiBearingY / 64.0;
//	glyph.mHorizontalAdvance = face->glyph->metrics.horiAdvance / 64.0;
//	glyph.mVerticalBearingX = face->glyph->metrics.vertBearingX / 64.0;
//	glyph.mVerticalBearingY = face->glyph->metrics.vertBearingY / 64.0;
//	glyph.mVerticalAdvance = face->glyph->metrics.vertAdvance / 64.0;
//	glyph.size = size;
//	glyph.time = timeFunc();
//
//	//face->glyph->metrics.ker
//
//	m_CashedData[fonttype].glyphs.insert(pair<long, Glyph>(unicode, glyph));
//	int a = 0;
//
//
//	msdfgen::destroyFont(font);
//	msdfgen::deinitializeFreetype(mft);
//
//	return true;
//}
//
//bool SDFGenerator::LoadGlyph(int fonttype, long unicode)
//{
//	int size = /*100*//*20*//*50*/30/*20*//*10*/;
//	char fontpath[256] = { 0 };
//	//sprintf(fontpath, "C:\\Windows\\Fonts\\%s.ttf", Support_Font_FileName[fonttype]);
//	//FT_Library ft;
//	//FT_Face face;
//	//FT_Init_FreeType(&ft);
//	//FT_New_Face(ft, /*"C:\\Windows\\Fonts\\malgun.ttf"*/fontpath, 0, &face);
//	//FT_Set_Pixel_Sizes(face, 0, size);
//	//FT_Load_Char(face, unicode, FT_LOAD_RENDER);
//
//	FT_Library ft;
//	FT_Face face;
//	FT_Init_FreeType(&ft);
//
//	if (fonttype == 0)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 0, &face);
//	}
//	else if (fonttype == 1)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 1, &face);
//	}
//	else if (fonttype == 2)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\malgun.ttf", 0, &face);
//	}
//	else if (fonttype == 3)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 2, &face);
//	}
//	else if (fonttype == 4)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\gulim.ttc", 3, &face);
//	}
//	else if (fonttype == 5)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\NGULIM.TTF", 0, &face);
//	}
//	else if (fonttype == 6)
//	{
//		FT_New_Face(ft, "C:\\Windows\\Fonts\\tahoma.ttf", 0, &face);
//	}
//
//	FT_Set_Pixel_Sizes(face, 0, size);
//	FT_Load_Char(face, unicode, FT_LOAD_RENDER);
//
//	FT_Bitmap& bitmap = face->glyph->bitmap;
//	int width = bitmap.width;
//	int height = bitmap.rows;
//
//	int Padding = /*16*//*6*//*10*/8/*6*//*4*/;
//	int newWidth = width + Padding * 2;
//	int newHeight = height + Padding * 2;
//	unsigned char* buffer2 = new unsigned char[newWidth * newHeight];
//	memset(buffer2, 0, newWidth * newHeight);
//
//	for (int y = 0; y < height; ++y)
//	{
//		for (int x = 0; x < width; ++x)
//		{
//			buffer2[(y + Padding) * newWidth + (x + Padding)] = bitmap.buffer[y * width + x];
//		}
//	}
//
//	width = newWidth;
//	height = newHeight;
//
//	float* sdf = new float[width * height];
//	//msdfgen::FreetypeHandle* ftt;
//	//msdfgen::FontHandle* font;
//	//msdfgen::Shape shape;
//	//{
//	//	
//	//	ftt = msdfgen::initializeFreetype();
//	//	font = msdfgen::loadFont(ftt, "C:\\Windows\\Fonts\\malgun.ttf");
//	//	
//
//	//	if (ftt != NULL)
//	//	{
//	//		if (font != NULL)
//	//		{
//	//			
//	//			if (msdfgen::loadGlyph(shape, font, unicode, msdfgen::FONT_SCALING_EM_NORMALIZED))
//	//			{
//	//				shape.normalize();
//	//				// SDF는 엣지 컬러링이 필요 없음
//	//				msdfgen::Bitmap<float, 1> bsdf(32, 32);
//	//				// 변환: 스케일 및 위치 조정
//	//				msdfgen::SDFTransformation t(msdfgen::Projection(32.0, msdfgen::Vector2(0.125, 0.125)), msdfgen::Range(0.125));
//	//				msdfgen::generateSDF(bsdf, shape, t);
//	//				// 파일 저장 없이 sdf.data()로 접근 가능
//	//				// DirectX9 텍스처 변환은 아래 참고
//
//	//				float* temp = (float*)bsdf;
//
//	//				width = 32;
//	//				height = 32;
//	//				memcpy(sdf, (const void*)temp, sizeof(float) * width * height);
//	//				
//	//				//CasheSDFTexture(zzz, width, height, &pos);
//	//			}
//	//		}
//	//	}
//	//	msdfgen::destroyFont(font);
//	//	msdfgen::deinitializeFreetype(ftt);
//	//}
//
//	GenerateSDF(/*bitmap.buffer*/buffer2, width, height, sdf);
//	//GenerateSDFAA(/*bitmap.buffer*/buffer2, width, height, sdf);
//
//	NormalizeAndQuantizeWithSpread(sdf, width * height, sdf, /*16*/7/*5*/);
//
//	NormalizeSDF(sdf, width * height);
//
//	std::vector<unsigned char> sdf8(width * height);
//	QuantizeSDF(sdf, width * height, sdf8.data());
//
//	POINT pos;
//	CasheSDFTextureRev(fonttype, sdf8.data(), width, height, &pos);
//
//
//	Glyph glyph;
//	glyph.mCodePoint = unicode;
//	glyph.mTextureCoordX = (float)(pos.x + Padding) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texwidth;
//	glyph.mTextureCoordY = (float)(pos.y + Padding) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texheight;
//	glyph.mTextureWidth = (float)(width - Padding * 2) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texwidth;
//	glyph.mTextureHeight = (float)(height - Padding * 2) / (float)/*SDF_CASH_SIZE*/m_CashedData[fonttype].texheight;
//	glyph.mWidth = face->glyph->metrics.width / 64.0;
//	glyph.mHeight = face->glyph->metrics.height / 64.0;
//	glyph.mHorizontalBearingX = face->glyph->metrics.horiBearingX / 64.0;
//	glyph.mHorizontalBearingY = face->glyph->metrics.horiBearingY / 64.0;
//	glyph.mHorizontalAdvance = face->glyph->metrics.horiAdvance / 64.0;
//	glyph.mVerticalBearingX = face->glyph->metrics.vertBearingX / 64.0;
//	glyph.mVerticalBearingY = face->glyph->metrics.vertBearingY / 64.0;
//	glyph.mVerticalAdvance = face->glyph->metrics.vertAdvance / 64.0;
//	glyph.size = size;
//	glyph.time = timeFunc();
//
//	//face->glyph->metrics.ker
//
//	m_CashedData[fonttype].glyphs.insert(pair<long, Glyph>(unicode, glyph));
//	int a = 0;
//
//
//	FT_Done_FreeType(ft);
//	//FT_Done_Face(face);
//
//	return true;
//}



void SDFGenerator::CasheSDFTextureRev(int index, const unsigned char* texdata, int width, int height, POINT* outpos, LPDIRECT3DTEXTURE9 debug)
{
	POINT pos = FindEmptyPos(m_CashedData[index].cashedCheck, m_CashedData[index].texwidth, m_CashedData[index].texheight, width, height);

	if (pos.x == -1 || pos.y == -1)
	{
		pos.x = m_CashedData[index].texwidth;
		pos.y = 0;
		ResizeSDFTextrue(index);
	}

	D3DLOCKED_RECT lockedRect;
	if (SUCCEEDED(m_CashedData[index].texture->LockRect(0, &lockedRect, nullptr, 0)))
	{
		unsigned char* dst = (unsigned char*)lockedRect.pBits;
		int dstPitch = lockedRect.Pitch;
		dst = dst + (pos.y * dstPitch + pos.x * 4);

		for (int y = height - 1; y >= 0; --y)
		{
			for (int x = 0; x < width; ++x)
			{
				unsigned char v = texdata[y * width + x];
				dst[x * 4 + 0] = v;
				dst[x * 4 + 1] = v;
				dst[x * 4 + 2] = v;
				dst[x * 4 + 3] = v;

				m_CashedData[index].cashedCheck[(pos.y + y) * m_CashedData[index].texwidth + (pos.x + x)] = 1;
			}
			dst += dstPitch;
		}
		m_CashedData[index].texture->UnlockRect(0);
	}

	outpos->x = pos.x;
	outpos->y = pos.y;

}


void SDFGenerator::CasheSDFTexture(int index, const unsigned char* texdata, int width, int height, POINT* outpos, LPDIRECT3DTEXTURE9 debug)
{
	POINT pos = FindEmptyPos(m_CashedData[index].cashedCheck, m_CashedData[index].texwidth, m_CashedData[index].texheight, width, height);

	if (pos.x == -1 || pos.y == -1)
	{
		pos.x = m_CashedData[index].texwidth;
		pos.y = 0;
		ResizeSDFTextrue(index);
	}

	D3DLOCKED_RECT lockedRect;
	if (SUCCEEDED(m_CashedData[index].texture->LockRect(0, &lockedRect, nullptr, 0)))
	{
		unsigned char* dst = (unsigned char*)lockedRect.pBits;
		int dstPitch = lockedRect.Pitch;
		dst = dst + (pos.y * dstPitch + pos.x * 4);

		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				unsigned char v = texdata[y * width + x];
				dst[x * 4 + 0] = v;
				dst[x * 4 + 1] = v;
				dst[x * 4 + 2] = v;
				dst[x * 4 + 3] = v;

				m_CashedData[index].cashedCheck[(pos.y + y) * m_CashedData[index].texwidth + (pos.x + x)] = 1;
			}
			dst += dstPitch;
		}
		m_CashedData[index].texture->UnlockRect(0);
	}

	outpos->x = pos.x;
	outpos->y = pos.y;

}

bool SDFGenerator::ResizeSDFTextrue(int index)
{
	LPDIRECT3DTEXTURE9 newTex;
	unsigned char* newCashed;
	unsigned char* oldCashed = m_CashedData[index].cashedCheck;

	int oldHeight = m_CashedData[index].texheight;
	int oldWidth = m_CashedData[index].texwidth;

	int newHeight = m_CashedData[index].texheight * 2;
	int newWidth = m_CashedData[index].texwidth * 2;

	if (FAILED(D3DXCreateTexture(m_device, newWidth, newHeight, 1, 0, D3DFORMAT::D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &newTex)))
	{
		return false;
	}

	newCashed = new unsigned char[newWidth * newHeight];
	ZeroMemory(newCashed, newWidth * newHeight);

	D3DLOCKED_RECT newRect;
	D3DLOCKED_RECT oldRect;
	if (SUCCEEDED(newTex->LockRect(0, &newRect, nullptr, 0)) && SUCCEEDED(m_CashedData[index].texture->LockRect(0, &oldRect, nullptr, 0)))
	{
		unsigned char* newbuf = (unsigned char*)newRect.pBits;
		int newPitch = newRect.Pitch;
		unsigned char* oldbuf = (unsigned char*)oldRect.pBits;
		int oldPitch = oldRect.Pitch;

		unsigned char* cashtemp = newCashed;

		for (int y = 0; y < oldHeight; y++)
		{
			memcpy(newbuf, oldbuf, oldPitch);
			newbuf += newPitch;
			oldbuf += oldPitch;


			memcpy(cashtemp, oldCashed, oldWidth);
			cashtemp += newWidth;
			oldCashed += oldWidth;
		}

		newTex->UnlockRect(0);
		m_CashedData[index].texture->UnlockRect(0);
	}

	float ratioX = (float)oldWidth / (float)newWidth;
	float ratioY = (float)oldHeight / (float)newHeight;

	for (map<long, Glyph>::iterator itr = m_CashedData[index].glyphs.begin(); itr != m_CashedData[index].glyphs.end(); itr++)
	{
		(*itr).second.mTextureHeight *= ratioY;
		(*itr).second.mTextureWidth *= ratioX;
		(*itr).second.mTextureCoordY *= ratioY;
		(*itr).second.mTextureCoordX *= ratioX;
	}

	m_CashedData[index].texture->Release();
	delete[] m_CashedData[index].cashedCheck;

	m_CashedData[index].texture = newTex;
	m_CashedData[index].cashedCheck = newCashed;
	m_CashedData[index].texwidth = newWidth;
	m_CashedData[index].texheight = newHeight;

	return true;
}

POINT SDFGenerator::DeleteOldGlyph(const unsigned char* tex, int w, int h, int w2, int h2)
{
	POINT pt = { -1,-1 };

	return pt;
}

POINT SDFGenerator::FindEmptyPos(const unsigned char* tex, int w, int h, int w2, int h2)
{
	POINT pt = { -1,-1 };

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			RECT rt = { x, y, x + w2, y + h2 };
			if ((x + w2) < w && (y + h2) < h && IsEmptyRect(tex, w, h, rt))
			{
				pt.x = x;
				pt.y = y;
				return pt;
			}
		}
	}

	return pt;
}

bool SDFGenerator::IsEmptyRect(const unsigned char* tex, int width, int height, RECT rt)
{
	for (int y = rt.top; y <= rt.bottom; y++)
	{
		for (int x = rt.left; x <= rt.right; x++)
		{
			if (tex[y * width + x] != 0)
			{
				return false;
			}
		}
	}

	return true;
}

// 유클리드 거리 계산
float SDFGenerator::dist(int x0, int y0, int x1, int y1)
{
	return std::sqrt((x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1));
}

// 외곽선 픽셀 판별
bool SDFGenerator::isEdge(const unsigned char* bmp, int x, int y, int w, int h)
{
	int idx = y * w + x;
	if (bmp[idx] < 128) return false;
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx)
		{
			if (dx == 0 && dy == 0) continue;
			int nx = x + dx, ny = y + dy;
			if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
			if (bmp[ny * w + nx] < 128) return true;
		}
	return false;
}

// SDF 생성 (브루트포스 방식, 정확도 우선)
void SDFGenerator::GenerateSDF(const unsigned char* bmp, int w, int h, float* sdf)
{
	/*unsigned char temp[1024] = { 0 };
	memcpy(temp, bmp, sizeof(char) * w * h);*/

	std::vector<std::pair<int, int>> edges;
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x)
			if (isEdge(bmp, x, y, w, h))
				edges.emplace_back(x, y);

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			bool inside = bmp[y * w + x] > 128;
			float minDist = (std::numeric_limits<float>::max)();
			for (auto& e : edges)
			{
				float d = dist(x, y, e.first, e.second);
				if (d < minDist) minDist = d;
			}
			sdf[y * w + x] = inside ? -minDist : minDist;
		}
	}
}

void SDFGenerator::NormalizeAndQuantizeWithSpread(const float* sdf, int size, float* out, float spread)
{
	for (int i = 0; i < size; ++i)
	{
		float v = sdf[i];
		// spread 범위로 클램프
		if (v < -spread) v = -spread;
		if (v > spread) v = spread;

		out[i] = v;
		// -spread ~ +spread → 0 ~ 255로 정규화
		/*float norm = (v + spread) / (2.0f * spread);
		out[i] = static_cast<unsigned char>(std::round(norm * 255.0f));*/
	}
}

void SDFGenerator::NormalizeSDF(float* sdf, int size)
{
	float minVal = *std::min_element(sdf, sdf + size);
	float maxVal = *std::max_element(sdf, sdf + size);
	for (int i = 0; i < size; ++i)
	{
		sdf[i] = 1.0 - (sdf[i] - minVal) / (maxVal - minVal);
	}
}

void SDFGenerator::NormalizeSDF3(float* sdf, int size)
{
	float minVal = *std::min_element(sdf, sdf + size);
	float maxVal = *std::max_element(sdf, sdf + size);
	for (int i = 0; i < size; ++i)
	{
		sdf[i] = (sdf[i] - minVal) / (maxVal - minVal);
	}
}

void SDFGenerator::QuantizeSDF(const float* sdf, int size, unsigned char* out)
{
	for (int i = 0; i < size; ++i)
	{
		out[i] = static_cast<unsigned char>(std::round(sdf[i] * 255.0f));
	}
}

void SDFGenerator::GenerateSDFAA(const unsigned char* bitmap, int width, int height, float* sdf)
{
	const float INF = 1e6f;
	std::vector<float> dist(width * height, INF);

	// 1. 외곽선 픽셀 찾기
	auto isEdge = [&](int x, int y)
	{
		int idx = y * width + x;
		if (bitmap[idx] < 128) return false; // 배경
		// 4방향 인접 픽셀 중 배경이 있으면 외곽선
		for (int dy = -1; dy <= 1; ++dy)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				if (dx == 0 && dy == 0) continue;
				int nx = x + dx, ny = y + dy;
				if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
				if (bitmap[ny * width + nx] < 128) return true;
			}
		}
		return false;
	};

	// 2. 외곽선 픽셀에 0, 나머지는 INF
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
			if (isEdge(x, y))
				dist[y * width + x] = 0.0f;

	// 3. Forward pass
	for (int y = 1; y < height; ++y)
	{
		for (int x = 1; x < width; ++x)
		{
			float d = dist[y * width + x];
			d = min(d, dist[(y - 1) * width + x] + 1.0f);
			d = min(d, dist[y * width + x - 1] + 1.0f);
			d = min(d, dist[(y - 1) * width + x - 1] + 1.4142f);
			dist[y * width + x] = d;
		}
	}

	// 4. Backward pass
	for (int y = height - 2; y >= 0; --y)
	{
		for (int x = width - 2; x >= 0; --x)
		{
			float d = dist[y * width + x];
			d = min(d, dist[(y + 1) * width + x] + 1.0f);
			d = min(d, dist[y * width + x + 1] + 1.0f);
			d = min(d, dist[(y + 1) * width + x + 1] + 1.4142f);
			dist[y * width + x] = d;
		}
	}

	// 5. 내부/외부 판별 및 signed 처리
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			int idx = y * width + x;
			// 내부면 음수, 외부면 양수
			bool inside = bitmap[idx] > 128;
			sdf[idx] = inside ? -dist[idx] : dist[idx];
		}
	}
}


//void SDFGenerator::ResizeGlyph(int fonttype, Glyph* glyph)
//{
//	float tempValue = 0.0f;
//	switch (fonttype)
//	{
//	case SDFFont::GULIM:
//	{
//		switch (glyph->mCodePoint)
//		{
//		case 'l':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'I':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '|':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'i':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'j':
//			glyph->mWidth *= /*1.5*/1.3;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'r':
//			glyph->mWidth *= /*1.3*/1.3;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'f':
//			glyph->mWidth *= /*1.3*/1.3;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '!':
//			glyph->mWidth *= /*1.5*/1.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '1':
//			glyph->mWidth *= 1.3;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case ';':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case ':':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '-':
//			glyph->mHeight *= /*1.5*/2.5;
//			break;
//		case '_':
//			glyph->mHeight *= /*1.5*/2.5;
//			break;
//		case '.':
//			glyph->mWidth *= /*1.5*/1.5;
//			glyph->mHeight *= /*1.5*/1.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case ',':
//			glyph->mWidth *= /*1.5*/1.5;
//			glyph->mHeight *= /*1.5*/1.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '\'':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '\"':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		}
//	}
//	break;
//
//	case SDFFont::GULIMCHE:
//		switch (glyph->mCodePoint)
//		{
//		case 'l':
//			glyph->mWidth *= /*2.0*/3.0;
//			tempValue = glyph->mWidth * 1.0;
//			glyph->mHorizontalAdvance += tempValue;
//
//			break;
//		case 'I':
//			glyph->mWidth *= /*2.0*/3.0;
//			tempValue = glyph->mWidth * 1.0;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '|':
//			glyph->mWidth *= /*2.0*/3.0;
//			tempValue = glyph->mWidth * 1.0;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'i':
//			glyph->mWidth *= /*1.3*/2.0;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'j':
//			glyph->mWidth *= /*1.3*/1.3;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'r':
//			glyph->mWidth *= /*1.3*/1.3;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'f':
//			glyph->mWidth *= /*1.3*/1.3;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '!':
//			glyph->mWidth *= /*1.3*/2.5;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '1':
//			glyph->mWidth *= /*1.3*/1.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case ';':
//			glyph->mWidth *= /*2.0*/2.5;
//			tempValue = glyph->mWidth * 1.0;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case ':':
//			glyph->mWidth *= /*2.0*/3.0;
//			tempValue = glyph->mWidth * 1.0;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '-':
//			glyph->mHeight *= /*1.5*/2.5;
//			break;
//		case '_':
//			glyph->mHeight *= /*1.5*/2.5;
//			break;
//		case '~':
//			glyph->mHeight *= /*1.5*/2.5;
//			break;
//		case '.':
//			glyph->mWidth *= /*1.5*/2.5;
//			glyph->mHeight *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case ',':
//			glyph->mWidth *= /*1.5*/1.5;
//			glyph->mHeight *= /*1.5*/1.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '\'':
//			glyph->mWidth *= /*2.5*/3.5;
//			tempValue = glyph->mWidth * 1.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '\"':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		}
//		break;
//
//		/*case MALGUN:*/
//	default:
//		switch (glyph->mCodePoint)
//		{
//		case 'l':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'I':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '|':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'i':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'j':
//			glyph->mWidth *= /*1.5*/1.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'r':
//			glyph->mWidth *= /*1.3*/1.3;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case 'f':
//			glyph->mWidth *= /*1.3*/1.3;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '!':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '1':
//			glyph->mWidth *= 1.3;
//			tempValue = glyph->mWidth * 0.3;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case ';':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case ':':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '-':
//			glyph->mHeight *= /*1.5*/2.5;
//			break;
//		case '_':
//			glyph->mHeight *= /*1.5*/2.5;
//			break;
//		case '.':
//			glyph->mWidth *= /*1.5*/1.5;
//			glyph->mHeight *= /*1.5*/1.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case ',':
//			glyph->mWidth *= /*1.5*/1.5;
//			glyph->mHeight *= /*1.5*/1.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '\'':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		case '\"':
//			glyph->mWidth *= /*1.5*/2.5;
//			tempValue = glyph->mWidth * 0.5;
//			glyph->mHorizontalAdvance += tempValue;
//			break;
//		}
//		break;
//	}
//}

#endif

