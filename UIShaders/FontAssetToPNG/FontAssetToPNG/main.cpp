#include <iostream>
#include <fstream>

//#pragma comment(lib, "lib/yaml-cppd.lib")
//#include "yaml-cpp/yaml.h"

#include "json/json.h"
#pragma comment(lib, "lib/jsoncpp.lib")
#pragma comment(lib, "lib/libpng.lib")
#pragma comment(lib, "lib/zlib.lib")
#include "libpng/png.h"

using namespace std;
void main()
{
	/*YAML::Node config = YAML::LoadFile("test.yaml");
	cout << config["MonoBehaviour"];*/
    Json::Value root;
    std::ifstream ifs("convertcsv.json", std::ifstream::in);
    Json::CharReaderBuilder builder;
    //builder["collectComments"] = false;
    JSONCPP_STRING errs;

    if (!parseFromStream(builder, ifs, &root, &errs)) {
        std::cout << errs << std::endl;
        return;
    }

    cout << root["MonoBehaviour"]["m_Name"] << endl;


    /*for (Json::ValueIterator itr = root.begin(); itr != root.end(); itr++)
    {
        cout << (*itr) << endl;
    }*/
    //16777216
    //33554432
    int Width = root["Texture2D"]["m_Width"].asInt();
    int Height = root["Texture2D"]["m_Height"].asInt();
    int ImageDataSize = root["Texture2D"]["image data"].asInt();


    FILE* pngOut; 
    fopen_s(&pngOut, "sdf.png", "wb");
    if (pngOut == nullptr) {

        cout << "파일 에러" << endl;
    }


    png_structp png_ptr = /*png_create_read_struct*/png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    if (png_ptr == nullptr) {

        cout << "에러..2." << endl;
    }
    png_infop pngInfoPtr = png_create_info_struct(png_ptr);
    if (pngInfoPtr == nullptr) {

        cout << "에러..3." << endl;
    }
    if (setjmp(png_jmpbuf(png_ptr)) != 0) {

        cout << "에러..4." << endl;
    }
    png_init_io(png_ptr, pngOut);

    if (setjmp(png_jmpbuf(png_ptr)) != 0) {

        cout << "에러..5." << endl;
    }
    png_set_IHDR(png_ptr,
        pngInfoPtr,
        125,
        125,
        8,
        PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE
    );

    //mPtrMain = (unsigned char*)malloc(sizeof(unsigned char) * 4 * len * len);
    unsigned char arr[125][125] = { 0 };
    unsigned char* mPtrMain = (unsigned char*)malloc(sizeof(unsigned char) * 4 * 125 * 125);
    unsigned char**  mPtrArray = (unsigned char**)malloc(sizeof(unsigned char*) * 125);
    for (auto i = 0; i < 125; i++) {

        mPtrArray[i] = &(mPtrMain[sizeof(unsigned char) * 4 * 125 * i]);
    }

    png_write_info(png_ptr, pngInfoPtr);

    if (setjmp(png_jmpbuf(png_ptr)) != 0) {

        cout << "에러..6." << endl;
    }
    png_write_image(png_ptr, mPtrArray);

    if (setjmp(png_jmpbuf(png_ptr)) != 0) {

        cout << "에러..7." << endl;
    }
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &pngInfoPtr);
    fclose(pngOut);


    /*if (mPtrMain != nullptr) {

        free(mPtrMain);
        free(mPtrArray);

        mPtrMain = nullptr;
        mPtrArray = nullptr;
    }*/
    //printf("Hello World!\n");
    //texture2D
    //material

}