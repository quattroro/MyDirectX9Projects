#pragma once


#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
//#pragma comment(lib, "library/jsoncpp.lib")

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.
// Windows 헤더 파일
#include <windows.h>
// C 런타임 헤더 파일입니다.
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include "Resource.h"


#include <memory.h>
#include <tchar.h>
#include <vector>
#include <map>
#include <string>
#include <queue>
#include <algorithm>
#include <iostream>
#include <fstream>

#include <d3d9.h>
#include <d3dx9.h>
#include <dinput.h>
#include <time.h>
#include <stack>


#include <commdlg.h>

#include "Vertex.h"
//#include "Engine.h"
#include "Transform.h"
//#include "json/json.h"





//static float g_AniFrameRate = 0.032;
extern int g_AniFrameRate;
extern double g_CurFrame;

using namespace std;
#define jmitest 10
#define sequencerfloat 10
#define FrameRatio 0.01


static bool cmp(struct SQ_Key a, struct SQ_Key b);

char teststr[256];
const char* CheckObjectName(const char* name);