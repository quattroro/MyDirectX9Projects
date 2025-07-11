#include "stdafx.h"
#include "SDFGenerator.h"

SDFGenerator::SDFGenerator(LPDIRECT3DDEVICE9 device)
{
	m_Device = device;
}

bool SDFGenerator::Init()
{
	if (FAILED(D3DXCreateTexture(m_Device, SDF_CASH_SIZE, SDF_CASH_SIZE, 1, 0, D3DFORMAT::D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_CashedSDFTexture)))
	{
		return false;
	}

	m_CashedCheck = new unsigned char[SDF_CASH_SIZE * SDF_CASH_SIZE];
	ZeroMemory(m_CashedCheck, SDF_CASH_SIZE * SDF_CASH_SIZE);

	return true;
}

Glyph SDFGenerator::GetCashedGlyph(long unicode)
{
	if (m_CashedGlyphs.count(unicode))
	{
		return m_CashedGlyphs[unicode];
	}
	else
	{
		LoadGlyph(unicode);
	}
}

void SDFGenerator::LoadGlyph(long unicode)
{
	//FT_Library ft;
	//FT_Face face;
	//FT_Init_FreeType(&ft);
	//FT_New_Face(ft, "C:\\Windows\\Fonts\\malgun.ttf", 0, &face);
	//FT_Set_Pixel_Sizes(face, 0, 50);
	//FT_Load_Char(face, unicode, FT_LOAD_RENDER);

	//FT_Bitmap& bitmap = face->glyph->bitmap;
	//int width = bitmap.width;
	//int height = bitmap.rows;

	//int Padding = 16;
	//int newWidth = width + Padding * 2;
	//int newHeight = height + Padding * 2;
	//unsigned char* buffer2 = new unsigned char[newWidth * newHeight];
	//memset(buffer2, 0, newWidth * newHeight);

	//for (int y = 0; y < height; ++y) {
	//	for (int x = 0; x < width; ++x) {
	//		buffer2[(y + Padding) * newWidth + (x + Padding)] = bitmap.buffer[y * width + x];
	//	}
	//}

	//width = newWidth;
	//height = newHeight;
	//
	//float* sdf = new float[width * height];
	//GenerateSDF(/*bitmap.buffer*/buffer2, width, height, sdf);

	//NormalizeAndQuantizeWithSpread(sdf, width * height, sdf, /*16*/7);
	////GenerateSDFAA(bitmap.buffer, width, height, sdf);

	//NormalizeSDF(sdf, width * height);

	//std::vector<unsigned char> sdf8(width * height);
	//QuantizeSDF(sdf, width * height, sdf8.data());

	//CasheSDFTexture(sdf8.data(), width, height);

	msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
	msdfgen::FontHandle* font = msdfgen::loadFont(ft, "C:\\Windows\\Fonts\\malgun.ttf");
	msdfgen::Shape shape;
	msdfgen::loadGlyph(shape, font, 'A', msdfgen::FONT_SCALING_EM_NORMALIZED);
	shape.normalize();
	msdfgen::edgeColoringSimple(shape, 3.0);
	msdfgen::Bitmap<float, 3> msdf(32, 32);
	msdfgen::SDFTransformation t(msdfgen::Projection(32.0, msdfgen::Vector2(0.125, 0.125)), msdfgen::Range(0.125));
	msdfgen::generateMSDF(msdf, shape, t);
	msdfgen::savePng(msdf, "output.png");


}

void SDFGenerator::CasheSDFTexture(const unsigned char* texdata, int width, int height)
{
	POINT pos = FindEmptyPos(m_CashedCheck, SDF_CASH_SIZE, SDF_CASH_SIZE, width, height);

	D3DLOCKED_RECT lockedRect;
	if (SUCCEEDED(m_CashedSDFTexture->LockRect(0, &lockedRect, nullptr, 0)))
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
				dst[x * 4 + 3] = 255;

				m_CashedCheck[(pos.y + y) * SDF_CASH_SIZE + (pos.x + x)] = 1;
			}
			dst += dstPitch;
		}
		m_CashedSDFTexture->UnlockRect(0);
	}
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

void SDFGenerator::QuantizeSDF(const float* sdf, int size, unsigned char* out)
{
	for (int i = 0; i < size; ++i)
	{
		out[i] = static_cast<unsigned char>(std::round(sdf[i] * 255.0f));
	}
}

