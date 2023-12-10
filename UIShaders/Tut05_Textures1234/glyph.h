#pragma once
#include <map>

class Glyph
{
public:
	long mCodePoint;
	float mWidth;
	float mHeight;
	float mHorizontalBearingX;
	float mHorizontalBearingY;
	float mHorizontalAdvance;
	float mVerticalBearingX;
	float mVerticalBearingY;
	float mVerticalAdvance;

	float mTextureCoordX;
	float mTextureCoordY;
	float mTextureWidth;
	float mTextureHeight;

	std::map<long, float> mKernings;
};