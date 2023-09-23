#include "generator_option_parser.hpp"
#include "generator_config.hpp"
#include "generator.hpp"

using namespace std;

int main ( int argc, char* argv[] )
{

    SDFont::GeneratorConfig conf;

    SDFont::GeneratorOptionParser parser( conf );

    bool res = parser.parse( argc, argv );

    if ( !res || parser.hasError() || parser.hasHelp() ) {

        cerr << parser.Usage;
        exit(1);
    }

    SDFont::Generator generator( conf, parser.hasVerbose() );

    setlocale ( LC_ALL, conf.locale().c_str() );

    res = generator.generate();

    if ( !res ) {

        cerr << parser.Usage;
        exit(1);
    }

    res = generator.emitFilePNG();

    if ( !res ) {

        cerr << parser.Usage;
        exit(1);
    }

    res = generator.emitFileMetrics();

    if ( !res ) {

        cerr << parser.Usage;
        exit(1);
    }

    if ( parser.hasVerbose() ) {

        cerr << "Finishied.\n";
    }

    return 0;
}
