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
//		//***Metrics���� ����***
//		//���� ��Ʈ ���� ���
//		//SDF �ؽ��� ũ��
//		//SDF �ؽ��� �ػ�
//		//SDF �ؽ��� ������
//		//SDF �ؽ��� ���� �̸�
//		//�� ����
//		//MARGIN ����
//		//GLYPHS ����
//		//KERNINGS ����
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
