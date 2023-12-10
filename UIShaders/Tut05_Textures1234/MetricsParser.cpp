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
//		//한줄씩 읽어온다.
//		while (!fin.eof())
//		{
//			string line;
//			getline(fin, line);
//
//			lineNumber++;
//
//
//			//유효한 줄인지 확인하고
//			if (isHeader(line))
//				continue;
//			//State를 변경해준다.
//			
//
//
//			//state에 따라서 데이터들을 파싱한다.
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
