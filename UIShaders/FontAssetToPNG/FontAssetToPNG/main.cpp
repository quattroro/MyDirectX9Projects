//#pragma comment(lib, "yaml-cpp.lib")
#pragma comment(lib, "lib/jsoncpp.lib")
#pragma comment(lib, "lib/libpng.lib")
#pragma comment(lib, "lib/zlib.lib")
#include <iostream>
#include <fstream>
//#include "JsonReader.h"

#include "json/json.h"
//#include "yaml-cpp/yaml.h"
#include "libpng/png.h"
using namespace std;

void ConvertTexture(Json::Value& root)
{
    int Width = root["Texture2D"]["m_Width"].asInt();
    int Height = root["Texture2D"]["m_Height"].asInt();
    int TexSize = root["Texture2D"]["image data"].asInt();

    const char* TexData = root["Texture2D"]["_typelessdata"].asCString();

    unsigned char* mPtrMain = (unsigned char*)malloc(sizeof(unsigned char) * 4 * Width * Height); //텍스쳐 배열 전체
    unsigned char** mPtrArray = (unsigned char**)malloc(sizeof(unsigned char*) * Height); // 텍스쳐 배열을 2차원 배열처럼 사용하기 위한 포인터 배열

    for (auto i = 0; i < Height; i++)
    {
        mPtrArray[i] = &(mPtrMain[sizeof(unsigned char) * 4 * Width * i]);
    }

    int ArrIndex = 0;
    int DataIndex = 0;
    const char* hex = TexData;
    for (int dataY = 0; dataY < Height; dataY++)
    {
        for (int dataX = 0; dataX < Width; dataX++)
        {
            char hex[3] = { 0 };
            memcpy(hex, TexData, sizeof(char) * 2);
            TexData = TexData + 2;
            int dec = (int)strtol(hex, NULL, 16);
            cout << "[" << dataY << "] [" << dataX << "]"" : " << dec << endl;
            for (int i = 0; i < 4; i++)
            {
                mPtrArray[dataY][(dataX * 4) + i] = dec;
            }
        }
    }



    //unsigned char* mPtrMain = (unsigned char*)malloc(sizeof(unsigned char) * 4 * 125 * 125); //텍스쳐 배열 전체
    //unsigned char** mPtrArray = (unsigned char**)malloc(sizeof(unsigned char*) * 125); // 텍스쳐 배열을 2차원 배열처럼 사용하기 위한 포인터 배열

    //for (auto i = 0; i < 125; i++) {

    //    mPtrArray[i] = &(mPtrMain[sizeof(unsigned char) * 4 * 125 * i]);
    //}
    /*int tempsize = strlen(mPtrMain);
    int tempsize2 = strlen(mPtrArray);*/

    FILE* pngOut = fopen("SDF_PNG.png", "wb");

    png_structp pngWritePtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    png_infop pngInfoPtr = png_create_info_struct(pngWritePtr);

    png_init_io(pngWritePtr, pngOut);

    png_set_IHDR(pngWritePtr, pngInfoPtr,
        Width,
        Height,
        8,
        PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE
    );

    png_write_info(pngWritePtr, pngInfoPtr);


    png_write_image(pngWritePtr, mPtrArray);


    png_write_end(pngWritePtr, NULL);

    png_destroy_write_struct(&pngWritePtr, &pngInfoPtr);

    fclose(pngOut);
}

void OutputMetricsHeader(ostream& os)
{

    os << "# Source Font Path: ";
    os << "sdf";
    os << "\n";
    os << "# Texture Size: ";
    os << "4096";
    os << "\n";
    os << "# Original Font Resolution: ";
    os << "4096";
    os << "\n";
    os << "# Down Sampling Scale: ";
    os << "1";
    os << "\n";
    os << "# Associated Texture File: ";
    os << "SDF_PNG" << ".png\n";
    os << "#\t";
    os << "Code Point";
    os << "\t";
    os << "Width";
    os << "\t";
    os << "Height";
    os << "\t";
    os << "Horizontal Bearing X";
    os << "\t";
    os << "Horizontal Bearing Y";
    os << "\t";
    os << "Horizontal Advance";
    os << "\t";
    os << "Vertical Bearing X";
    os << "\t";
    os << "Vertical Bearing Y";
    os << "\t";
    os << "Vertical Advance";
    os << "\t";
    os << "Texture Coord X";
    os << "\t";
    os << "Texture Coord Y";
    os << "\t";
    os << "Texture Width";
    os << "\t";
    os << "Texture Height";
    os << "\n";
}

struct GlyphData
{
    int m_Index;
    float m_Width;
    float m_Height;
    float m_HorizontalBearingX;
    float m_HorizontalBearingY;
    float m_HorizontalAdvance;

    float m_X;
    float m_Y;
    float m_texWidth;
    float m_texHeight;

};

map<int, GlyphData> GlyphMap;

struct CharacterData
{
    int m_Index;
    int m_Unicode;
};

map<int, CharacterData> CharacterMap;

void ConvertMetrixData(Json::Value& root)
{
    int Width = root["Texture2D"]["m_Width"].asInt();
    int Height = root["Texture2D"]["m_Height"].asInt();
    int TexSize = root["Texture2D"]["image data"].asInt();

    ofstream osMetrics("SDF_PNG.txt");
    OutputMetricsHeader(osMetrics);

    osMetrics << "MARGIN\n";
    osMetrics << root["MonoBehaviour"]["m_CreationSettings"]["padding"].asFloat() / (float)Width;
    osMetrics << "\n";

    osMetrics << "GLYPHS\n";





    /*{
        "m_Index": 71,
            "m_Metrics" : {
            "m_Width": 0,
                "m_Height" : 0,
                "m_HorizontalBearingX" : 0,
                "m_HorizontalBearingY" : 0,
                "m_HorizontalAdvance" : 9.984375
        },
            "m_GlyphRect": {
                "m_X": 0,
                    "m_Y" : 0,
                    "m_Width" : 0,
                    "m_Height" : 0
            },
                "m_Scale": 1,
                    "m_AtlasIndex" : 0
    },*/

    /*{
        "m_ElementType": 1,
            "m_Unicode" : 32,
            "m_GlyphIndex" : 71,
            "m_Scale" : 1
    },*/

    //공백문자 u0020 -> 32

    //"m_GlyphTable" 값이 조합되어야 한다.
    //"m_CharacterTable" 값과

    Json::Value GlyphTable = root["MonoBehaviour"]["m_GlyphTable"];
    Json::ValueIterator GlyphItr = GlyphTable.begin();
    Json::Value CharacterTable = root["MonoBehaviour"]["m_CharacterTable"];
    Json::ValueIterator CharacterItr = CharacterTable.begin();


    //osMetrics << "글리프 글자 갯수 " << GlyphTable.size() << endl;
    //osMetrics << "총 글자 갯수 " << CharacterTable.size() << endl;

    int a = 0;

    for (int i = 0; i < GlyphTable.size(); i++)
    {
        GlyphData tempdata;
        tempdata.m_Index = (*GlyphItr)["m_Index"].asInt();
        tempdata.m_Width = (*GlyphItr)["m_Metrics"]["m_Width"].asFloat();
        tempdata.m_Height = (*GlyphItr)["m_Metrics"]["m_Height"].asFloat();
        tempdata.m_HorizontalBearingX = (*GlyphItr)["m_Metrics"]["m_HorizontalBearingX"].asFloat();
        tempdata.m_HorizontalBearingY = (*GlyphItr)["m_Metrics"]["m_HorizontalBearingY"].asFloat();
        tempdata.m_HorizontalAdvance = (*GlyphItr)["m_Metrics"]["m_HorizontalAdvance"].asFloat();

        tempdata.m_X = (*GlyphItr)["m_GlyphRect"]["m_X"].asFloat();
        tempdata.m_Y = (*GlyphItr)["m_GlyphRect"]["m_Y"].asFloat();
        tempdata.m_texWidth = (*GlyphItr)["m_GlyphRect"]["m_Width"].asFloat();
        tempdata.m_texHeight = (*GlyphItr)["m_GlyphRect"]["m_Height"].asFloat();

        GlyphMap[tempdata.m_Index] = tempdata;
        //GlyphVector.push_back(tempdata);

        ++GlyphItr;
    }

    for (int i = 0; i < CharacterTable.size(); i++)
    {
        CharacterData tempdata;
        tempdata.m_Index = (*CharacterItr)["m_GlyphIndex"].asInt();
        tempdata.m_Unicode = (*CharacterItr)["m_Unicode"].asInt();

        CharacterMap[tempdata.m_Index] = tempdata;
        //CharacterVector.push_back(tempdata);

        ++CharacterItr;
    }


    map<int, CharacterData>::iterator itr = CharacterMap.begin();
    while (true)
    {
        if (itr == CharacterMap.end())
            break;

        if (GlyphMap.find(itr->first) != GlyphMap.end())
        {
            osMetrics /*<< "m_Unicode : " */ << itr->second.m_Unicode << "\t";

            osMetrics /*<< "m_Width : "*/ << GlyphMap[itr->first].m_Width << "\t";
            osMetrics /*<< "m_Height : "*/ << GlyphMap[itr->first].m_Height << "\t";
            osMetrics /*<< "m_HorizontalBearingX : "*/ << GlyphMap[itr->first].m_HorizontalBearingX << "\t";
            osMetrics /*<< "m_HorizontalBearingY : "*/ << GlyphMap[itr->first].m_HorizontalBearingY << "\t";
            osMetrics /*<< "m_HorizontalAdvance : "*/ << GlyphMap[itr->first].m_HorizontalAdvance << "\t";

            osMetrics /*<< "m_VerticalBearingX : "*/ << 0 << "\t";
            osMetrics /*<< "m_VerticalBearingY: "*/ << 0 << "\t";
            osMetrics /*<< "m_VerticalAdvance : "*/ << 0 << "\t";

            osMetrics /*<< "m_X : "*/ << GlyphMap[itr->first].m_X / (float)Width << "\t";
            osMetrics /*<< "m_Y : "*/ << GlyphMap[itr->first].m_Y / (float)Height << "\t";
            osMetrics /*<< "m_Width : "*/ << GlyphMap[itr->first].m_texWidth / (float)Width << "\t";
            osMetrics /*<< "m_Height : "*/ << GlyphMap[itr->first].m_texHeight / (float)Height << "\t";

        }
        else//인덱스 존재 하지 않음
        {
            /*osMetrics << "m_Unicode : " << itr->second.m_Unicode << "\t";
            osMetrics << "인덱스 존재하지 않음";*/
        }

        osMetrics << endl;
        ++itr;
    }

    osMetrics << "#Kernings\tPred Code Point\tSucc Code Point 1"
        "\tKerning1\tSucc Code Point 2\tKerning 2...\n";

    osMetrics << "KERNINGS\n";


    //for (int i=0;i< GlyphTable.size();i++)
    //{
    //    if (GlyphItr == GlyphTable.end() || CharacterItr == CharacterTable.end())
    //        break;

    //    /*if (a > 300)
    //        break;*/

    //    osMetrics << "m_GlyphIndex : " << (*CharacterItr)["m_GlyphIndex"].asInt() << "\t";
    //    osMetrics << "m_Unicode : " << (*CharacterItr)["m_Unicode"].asString() << "\t";

    //    //(*GlyphItr)["m_Index"].compare()
    //    osMetrics << "m_Index : " <<(*GlyphItr)["m_Index"].asInt() << "\t";
    //    osMetrics << "m_Width : " << (*GlyphItr)["m_Metrics"]["m_Width"].asString() << "\t";
    //    osMetrics << "m_Height : " << (*GlyphItr)["m_Metrics"]["m_Height"].asString() << "\t";
    //    osMetrics << "m_HorizontalBearingX : " << (*GlyphItr)["m_Metrics"]["m_HorizontalBearingX"].asString() << "\t";
    //    osMetrics << "m_HorizontalBearingY : " << (*GlyphItr)["m_Metrics"]["m_HorizontalBearingY"].asString() << "\t";
    //    osMetrics << "m_HorizontalAdvance : " << (*GlyphItr)["m_Metrics"]["m_HorizontalAdvance"].asString() << "\t";

    //    osMetrics << "m_X : " << (*GlyphItr)["m_GlyphRect"]["m_X"].asFloat() / (float)Width << "\t";
    //    osMetrics << "m_Y : " << (*GlyphItr)["m_GlyphRect"]["m_Y"].asFloat() / (float)Height << "\t";
    //    osMetrics << "m_Width : " << (*GlyphItr)["m_GlyphRect"]["m_Width"].asFloat() / (float)Width << "\t";
    //    osMetrics << "m_Height : " << (*GlyphItr)["m_GlyphRect"]["m_Height"].asFloat() / (float)Height << "\t";

    //    ++CharacterItr;
    //    ++GlyphItr;
    //    a++;

    //    osMetrics << endl;
    //}

    osMetrics.close();
}

void main()
{
    ifstream  fin("convertcsv.json");
    if (!fin.is_open())
    {
        cout << "파일 열기 실패" << endl;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    //builder["collextComments"] = true;
    JSONCPP_STRING errs;
    if (!parseFromStream(builder, fin, &root, &errs))
    {
        std::cout << errs << std::endl;
        return;
    }
    fin.close();
    int a = 0;
    for (Json::ValueIterator itr = root.begin(); itr != root.end(); itr++)
    {
        a++;
        cout << "멤버 개수  " << a << endl;
        /*cout << (*itr) << endl;
        _sleep(9000);*/

    }

    cout << root["MonoBehaviour"]["m_Name"].asString() << endl;
    cout << root["MonoBehaviour"]["m_Version"].asString() << endl;
    cout << root["Texture2D"]["image data"].asInt() << endl;


    ConvertTexture(root);
    ConvertMetrixData(root);


    //int Width = root["Texture2D"]["m_Width"].asInt();
    //int Height = root["Texture2D"]["m_Height"].asInt();
    //int TexSize = root["Texture2D"]["image data"].asInt();

    //const char* TexData = root["Texture2D"]["_typelessdata"].asCString();

    //unsigned char* mPtrMain = (unsigned char*)malloc(sizeof(unsigned char) * 4 * Width * Height); //텍스쳐 배열 전체
    //unsigned char** mPtrArray = (unsigned char**)malloc(sizeof(unsigned char*) * Height); // 텍스쳐 배열을 2차원 배열처럼 사용하기 위한 포인터 배열

    //for (auto i = 0; i < Height; i++)
    //{
    //    mPtrArray[i] = &(mPtrMain[sizeof(unsigned char) * 4 * Width * i]);
    //}

    //int ArrIndex = 0;
    //int DataIndex = 0;
    //const char* hex = TexData;
    //for (int dataY = 0; dataY < Height; dataY++)
    //{
    //    for (int dataX = 0; dataX < Width; dataX++)
    //    {
    //        char hex[3] = { 0 };
    //        memcpy(hex, TexData, sizeof(char) * 2);
    //        TexData = TexData + 2;
    //        int dec = (int)strtol(hex, NULL, 16);
    //        cout << "[" << dataY << "] [" << dataX << "]"" : " << dec << endl;
    //        for (int i = 0; i < 4; i++)
    //        {
    //            mPtrArray[dataY][(dataX * 4) + i] = dec;
    //        }
    //    }
    //}



    ////unsigned char* mPtrMain = (unsigned char*)malloc(sizeof(unsigned char) * 4 * 125 * 125); //텍스쳐 배열 전체
    ////unsigned char** mPtrArray = (unsigned char**)malloc(sizeof(unsigned char*) * 125); // 텍스쳐 배열을 2차원 배열처럼 사용하기 위한 포인터 배열

    ////for (auto i = 0; i < 125; i++) {

    ////    mPtrArray[i] = &(mPtrMain[sizeof(unsigned char) * 4 * 125 * i]);
    ////}
    ///*int tempsize = strlen(mPtrMain);
    //int tempsize2 = strlen(mPtrArray);*/

    //FILE* pngOut = fopen("SDF_PNG.png", "wb");

    //png_structp pngWritePtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    //png_infop pngInfoPtr = png_create_info_struct(pngWritePtr);

    //png_init_io(pngWritePtr, pngOut);

    //png_set_IHDR(pngWritePtr,pngInfoPtr,
    //    Width,
    //    Height,
    //    8,
    //    PNG_COLOR_TYPE_RGB_ALPHA,
    //    PNG_INTERLACE_NONE,
    //    PNG_COMPRESSION_TYPE_BASE,
    //    PNG_FILTER_TYPE_BASE
    //);

    //png_write_info(pngWritePtr, pngInfoPtr);


    //png_write_image(pngWritePtr, mPtrArray);


    //png_write_end(pngWritePtr, NULL);

    //png_destroy_write_struct(&pngWritePtr, &pngInfoPtr);

    //fclose(pngOut);

    /*if (mPtrMain != nullptr) {

        free(mPtrMain);
        free(mPtrArray);

        mPtrMain = nullptr;
        mPtrArray = nullptr;
    }*/

    //cout << root["Texture2D"]["_typelessdata"].asString() << endl;
    //Material

    /*fin >> root;
    fin.close();*/
    /*Json::Reader reader123;
    reader123.parse(fin, root);

    cout << root["miejs"] << endl;

    for (auto& value : root)
        cout << value << endl;
    for (auto& value : root["string_key"])
        cout << value.asString() << endl;*/





        //root.get("m_Name")

        /*cout << root["m_Name"] << endl;
        if (root["material"].isMember("fileID"))
        {
            cout << "검색됨" << endl;
            cout << root["material"]["fileID"] << endl;
        }*/




        /*ifstream fin("test.yaml");
        YAML::Parser parser;
        parser.Load(fin);*/

        //YAML::Node doc;
        //YAML::EventHandler mhandle

        /*while (parser.HandleNextDocument(mhandle))
        {

        }*/



}




//#include <iostream>
//#include <fstream>
//
////#pragma comment(lib, "lib/yaml-cppd.lib")
////#include "yaml-cpp/yaml.h"
//
//#include "json/json.h"
//#pragma comment(lib, "lib/jsoncpp.lib")
//#pragma comment(lib, "lib/libpng.lib")
//#pragma comment(lib, "lib/zlib.lib")
//#include "libpng/png.h"
//
//using namespace std;
//void main()
//{
//	/*YAML::Node config = YAML::LoadFile("test.yaml");
//	cout << config["MonoBehaviour"];*/
//    Json::Value root;
//    std::ifstream ifs("convertcsv.json", std::ifstream::in);
//    Json::CharReaderBuilder builder;
//    //builder["collectComments"] = false;
//    JSONCPP_STRING errs;
//
//    if (!parseFromStream(builder, ifs, &root, &errs)) {
//        std::cout << errs << std::endl;
//        return;
//    }
//
//    cout << root["MonoBehaviour"]["m_Name"] << endl;
//
//
//    /*for (Json::ValueIterator itr = root.begin(); itr != root.end(); itr++)
//    {
//        cout << (*itr) << endl;
//    }*/
//    //16777216
//    //33554432
//    int Width = root["Texture2D"]["m_Width"].asInt();
//    int Height = root["Texture2D"]["m_Height"].asInt();
//    int ImageDataSize = root["Texture2D"]["image data"].asInt();
//
//
//    FILE* pngOut; 
//    fopen_s(&pngOut, "sdf.png", "wb");
//    if (pngOut == nullptr) {
//
//        cout << "파일 에러" << endl;
//    }
//
//
//    png_structp png_ptr = /*png_create_read_struct*/png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
//    
//    if (png_ptr == nullptr) {
//
//        cout << "에러..2." << endl;
//    }
//    png_infop pngInfoPtr = png_create_info_struct(png_ptr);
//    if (pngInfoPtr == nullptr) {
//
//        cout << "에러..3." << endl;
//    }
//    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
//
//        cout << "에러..4." << endl;
//    }
//    png_init_io(png_ptr, pngOut);
//
//    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
//
//        cout << "에러..5." << endl;
//    }
//    png_set_IHDR(png_ptr,
//        pngInfoPtr,
//        125,
//        125,
//        8,
//        PNG_COLOR_TYPE_RGB_ALPHA,
//        PNG_INTERLACE_NONE,
//        PNG_COMPRESSION_TYPE_BASE,
//        PNG_FILTER_TYPE_BASE
//    );
//
//    //mPtrMain = (unsigned char*)malloc(sizeof(unsigned char) * 4 * len * len);
//    unsigned char arr[125][125] = { 0 };
//    unsigned char* mPtrMain = (unsigned char*)malloc(sizeof(unsigned char) * 4 * 125 * 125);
//    unsigned char**  mPtrArray = (unsigned char**)malloc(sizeof(unsigned char*) * 125);
//    for (auto i = 0; i < 125; i++) {
//
//        mPtrArray[i] = &(mPtrMain[sizeof(unsigned char) * 4 * 125 * i]);
//    }
//
//    png_write_info(png_ptr, pngInfoPtr);
//
//    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
//
//        cout << "에러..6." << endl;
//    }
//    png_write_image(png_ptr, mPtrArray);
//
//    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
//
//        cout << "에러..7." << endl;
//    }
//    png_write_end(png_ptr, NULL);
//    png_destroy_write_struct(&png_ptr, &pngInfoPtr);
//    fclose(pngOut);
//
//
//    /*if (mPtrMain != nullptr) {
//
//        free(mPtrMain);
//        free(mPtrArray);
//
//        mPtrMain = nullptr;
//        mPtrArray = nullptr;
//    }*/
//    //printf("Hello World!\n");
//    //texture2D
//    //material
//
//}