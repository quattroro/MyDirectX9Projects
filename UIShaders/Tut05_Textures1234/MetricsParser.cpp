//#include "MetricsParser.h"
//#include <fstream>
//#include <sstream>
//
//namespace SDFFont
//{
//	void MetricsParser::parseMetrics(string fileName)
//	{
//		ifstream fin;
//		fin.open(fileName);
//
//		long lineNumber = 0;
//		enum parseState state = E_INIT;
//
//		//���پ� �о�´�.
//		while (!fin.eof())
//		{
//			string line;
//			getline(fin, line);
//
//			lineNumber++;
//
//
//			//��ȿ�� ������ Ȯ���ϰ�
//			if (isHeader(line))
//				continue;
//			//State�� �������ش�.
//			
//
//
//			//state�� ���� �����͵��� �Ľ��Ѵ�.
//
//
//
//
//
//		}
//
//
//		
//
//	}
//
//	bool MetricsParser::isHeader(string line)
//	{
//		return (line.at(0) == '#');
//	}
//
//	
//
//	bool MetricsParser::setState(string line, parseState& state)
//	{
//		
//	}
//
//	void MetricsParser::parseMargin(string line, long lineNumber, bool& error)
//	{
//	}
//
//	void MetricsParser::parseGlyphs(string line, long lineNumber, bool& error)
//	{
//	}
//
//	void MetricsParser::parseKernings(string line, long lineNumber, bool& error)
//	{
//	}
//
//	
//
//
//}
