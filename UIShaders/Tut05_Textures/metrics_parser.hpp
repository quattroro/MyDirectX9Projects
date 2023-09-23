#ifndef __METRICS_PARSER_HPP__
#define __METRICS_PARSER_HPP__

#include <map>
#include <string>
#include "glyph.hpp"
#include <vector>
using namespace std;

namespace SDFont {

class MetricsParser {

  public:


    /** @brief constructor
     *
     *  @param  G (in/out) graph to which nodes and edges are to be added
     */
    MetricsParser( map< long, Glyph>& glyphs, float& margin ) :
        mMargin(margin),
        mGlyphs(glyphs) {;}


    virtual ~MetricsParser(){;}


    /** @brief
     *
     *  @param  filename (in): name of the file to be opened and parsed.
     *
     */
    bool parseSpec( string fileName );

    static const string MARGIN;
    static const string GLYPHS;
    static const string KERNINGS;

  private:

    enum parseState {
        INIT,
        IN_MARGIN,
        IN_GLYPHS,
        IN_KERNINGS,
        END
    };


    void trim( string& line );


    bool isSectionHeader( string line, enum parseState& state );


    bool isCommentLine  ( string line );


    size_t splitLine    ( const string& txt, vector<string>& strs, char ch );


    void handleMargin(
        string line,
        string filename,
        long   lineNumber,
        bool&  errorFlag
    );


    void handleGlyph(
        string line,
        string filename,
        long   lineNumber,
        bool&  errorFlag
    );


    void handleKerning(
        string line,
        string fileName,
        long   lineNumber,
        bool&  errorFlag
    );


    void emitError(
        string fileName,
        long   lineNumber,
        string mess,
        bool&  errorFlag
    );


    float& mMargin;


    /** @brief used during parsing to find a node from a node number.*/
    map< long, Glyph >& mGlyphs;

};


} // namespace SDFont

#endif /*__METRICS_PARSER_HPP__*/
