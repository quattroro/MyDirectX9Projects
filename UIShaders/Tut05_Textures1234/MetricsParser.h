//#pragma once
//#include <iostream>
//
//#include <map>
////#include "glyph.h"
//
//
//using namespace std;
//
//
//namespace SDFFont
//{
//	class MetricsParser
//	{
//		//***Metrics파일 구성***
//		//원본 폰트 파일 경로
//		//SDF 텍스쳐 크기
//		//SDF 텍스쳐 해상도
//		//SDF 텍스쳐 스케일
//		//SDF 텍스쳐 파일 이름
//		//열 제목
//		//MARGIN 정보
//		//GLYPHS 정보
//		//KERNINGS 정보
//
//		enum parseState
//		{
//			E_INIT,
//			E_MARGIN,
//			E_GLYPHS,
//			E_KERNINGS,
//			E_END
//		};
//
//
//		void parseMetrics(string fileName);
//
//		bool isHeader(string line);
//
//		bool setState(string line, parseState& state);
//		void parseMargin(string line, long lineNumber, bool& error);
//		void parseGlyphs(string line, long lineNumber, bool& error);
//		void parseKernings(string line, long lineNumber, bool& error);
//
//		float& mMargin;
//		map<long, class Glyph>& mGlyphs;
//	};
//}
//
//
//
