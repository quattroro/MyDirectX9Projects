#include "stdafx.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "metrics_parser.h"

namespace SDFont {

const std::string MetricsParser::MARGIN   = "MARGIN";
const std::string MetricsParser::GLYPHS   = "GLYPHS";
const std::string MetricsParser::KERNINGS = "KERNINGS";


void MetricsParser::trim( string& line ) {

    if ( !line.empty() && line[line.size() - 1] == '\n' ) {

        line.erase(line.size() - 1);
    }

    if ( !line.empty() && line[line.size() - 1] == '\r' ) {

        line.erase(line.size() - 1);
    }
}


bool MetricsParser::parseSpec( string fileName )
{

    ifstream        is( fileName.c_str() );

    if (!is) {
        return false;
    }

    long            lineNumber = 0;
    bool            error      = false;
    enum parseState state      = INIT;

    while ( !is.eof() && !error ) {

        string line;

        getline( is, line );

        trim( line );

        lineNumber++;

        if ( line.empty() ) {
            continue;
        }

        if ( isCommentLine(line) ) {
            continue;
        }

        if( isSectionHeader( line, state ) ) {
            continue;
        }

        switch( state ) {

          case INIT:

            emitError     ( fileName, lineNumber, "", error );
            break;

          case IN_MARGIN:

            handleMargin  ( line, fileName, lineNumber, error );
            break;

          case IN_GLYPHS:

            handleGlyph   ( line, fileName, lineNumber, error );
            break;

          case IN_KERNINGS:

            handleKerning ( line, fileName, lineNumber, error );
            break;

          case END:
          default:

            emitError     ( fileName, lineNumber, "", error );
            break;
        }
    }

    if (error) {
        return false;
    }

    return true;
}


bool MetricsParser::isSectionHeader (

    string           line,
    enum parseState& state

) {

    if ( line.compare( 0, MARGIN.size(), MARGIN ) == 0 ) {

        state = IN_MARGIN;
        return true;
    }
    else if ( line.compare( 0, GLYPHS.size(), GLYPHS ) == 0 ) {

        state = IN_GLYPHS;
        return true;
    }
    else if ( line.compare( 0, KERNINGS.size(), KERNINGS ) == 0 ) {

        state = IN_KERNINGS;
        return true;
    }

    return false;
}


void MetricsParser::emitError(

    string fileName,
    long   lineNumber,
    string message ,
    bool&  errorFlag

) {

    cerr << "Syntax Error: "
         << fileName
         << " at line: "
         << lineNumber
         << " "
         << message
         << "\n";

    errorFlag = true;
}


bool MetricsParser::isCommentLine( std::string line )
{
    return line.at(0) == '#';
}


size_t MetricsParser::splitLine(

    const string&   txt,
    vector<string>& strs,
    char            ch

) {

    auto   pos        = txt.find( ch );
    size_t initialPos = 0;

    strs.clear();

    while( pos != std::string::npos && initialPos < txt.size() ) {

        if ( pos > initialPos ) {

            strs.push_back( txt.substr( initialPos, pos - initialPos ) );
        }

        initialPos = pos + 1;

        if ( initialPos < txt.size() ) {
            pos = txt.find( ch, initialPos );
        }

    }

    if ( initialPos < txt.size() ) {

        strs.push_back( txt.substr( initialPos, txt.size() - initialPos ) );
    }

    return strs.size();
}



void MetricsParser::handleMargin (

    string line,
    string filename,
    long   lineNumber,
    bool&  errorFlag

) {

    vector<std::string> fields;

    if ( splitLine( line, fields, '\t' ) != 1 ) {

        emitError( filename, lineNumber, "Invalid Margin", errorFlag );
        return;
    }

    Glyph g;

    mMargin = stof( fields[ 0] );
}


void MetricsParser::handleGlyph (

    string line,
    string filename,
    long   lineNumber,
    bool&  errorFlag

) {

    vector<std::string> fields;

    if ( splitLine( line, fields, '\t' ) != 13 ) {

        emitError( filename, lineNumber, "Invalid Node", errorFlag );
        return;
    }

    Glyph g;

    g.mCodePoint          = stol( fields[ 0] );
    g.mWidth              = stof( fields[ 1] );
    g.mHeight             = stof( fields[ 2] );
    g.mHorizontalBearingX = stof( fields[ 3] );
    g.mHorizontalBearingY = stof( fields[ 4] );
    g.mHorizontalAdvance  = stof( fields[ 5] );
    g.mVerticalBearingX   = stof( fields[ 6] );
    g.mVerticalBearingY   = stof( fields[ 7] );
    g.mVerticalAdvance    = stof( fields[ 8] );
    g.mTextureCoordX      = stof( fields[ 9] );
    g.mTextureCoordY      = stof( fields[10] );
    g.mTextureWidth       = stof( fields[11] );
    g.mTextureHeight      = stof( fields[12] );

    mGlyphs[ g.mCodePoint ] = g;

}


void MetricsParser::handleKerning (

    string      line,
    string      filename,
    long        lineNumber,
    bool&       errorFlag

) {

    vector<std::string> fields;

    auto numFields = splitLine( line, fields, '\t' );

    if ( numFields < 3 || (numFields - 1) % 2 != 0 ) {

        emitError( filename, lineNumber, "Invalid Node", errorFlag );
    }

    auto& g = mGlyphs[ stol( fields[ 0] ) ];

    for ( int i = 1; i < numFields; i +=2 ) {

        g.mKernings[ stol( fields[ i ]) ] = stof( fields[ i+1 ] );
    }
}


} // namespace SDFont
