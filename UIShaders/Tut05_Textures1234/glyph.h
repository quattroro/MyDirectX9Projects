#pragma once
#include <map>

class Glyph
{
public:
	Glyph()
	{
		mCodePoint = 0;
		mWidth = 0;
		mHeight = 0;
		mHorizontalBearingX = 0;
		mHorizontalBearingY = 0;
		mHorizontalAdvance = 0;
		mVerticalBearingX = 0;
		mVerticalBearingY = 0;
		mVerticalAdvance = 0;

		mTextureCoordX = 0;
		mTextureCoordY = 0;
		mTextureWidth = 0;
		mTextureHeight = 0;

	}

	Glyph(Glyph* pglyph)
	{
		mCodePoint = pglyph->mCodePoint;
		mWidth = pglyph->mWidth;
		mHeight = pglyph->mHeight;
		mHorizontalBearingX = pglyph->mHorizontalBearingX;
		mHorizontalBearingY = pglyph->mHorizontalBearingY;
		mHorizontalAdvance = pglyph->mHorizontalAdvance;
		mVerticalBearingX = pglyph->mVerticalBearingX;
		mVerticalBearingY = pglyph->mVerticalBearingY;
		mVerticalAdvance = pglyph->mVerticalAdvance;

		mTextureCoordX = pglyph->mTextureCoordX;
		mTextureCoordY = pglyph->mTextureCoordY;
		mTextureWidth = pglyph->mTextureWidth;
		mTextureHeight = pglyph->mTextureHeight;

		for (pair<long, float> k : pglyph->mKernings)
		{
			mKernings.insert(pair<long, float>(k.first, k.second));
		}
	}

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