//
//     Copyright (C) Pixar. All rights reserved.
//
//     This license governs use of the accompanying software. If you
//     use the software, you accept this license. If you do not accept
//     the license, do not use the software.
//
//     1. Definitions
//     The terms "reproduce," "reproduction," "derivative works," and
//     "distribution" have the same meaning here as under U.S.
//     copyright law.  A "contribution" is the original software, or
//     any additions or changes to the software.
//     A "contributor" is any person or entity that distributes its
//     contribution under this license.
//     "Licensed patents" are a contributor's patent claims that read
//     directly on its contribution.
//
//     2. Grant of Rights
//     (A) Copyright Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free copyright license to reproduce its contribution,
//     prepare derivative works of its contribution, and distribute
//     its contribution or any derivative works that you create.
//     (B) Patent Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free license under its licensed patents to make, have
//     made, use, sell, offer for sale, import, and/or otherwise
//     dispose of its contribution in the software or derivative works
//     of the contribution in the software.
//
//     3. Conditions and Limitations
//     (A) No Trademark License- This license does not grant you
//     rights to use any contributor's name, logo, or trademarks.
//     (B) If you bring a patent claim against any contributor over
//     patents that you claim are infringed by the software, your
//     patent license from such contributor to the software ends
//     automatically.
//     (C) If you distribute any portion of the software, you must
//     retain all copyright, patent, trademark, and attribution
//     notices that are present in the software.
//     (D) If you distribute any portion of the software in source
//     code form, you may do so only under this license by including a
//     complete copy of this license with your distribution. If you
//     distribute any portion of the software in compiled or object
//     code form, you may only do so under a license that complies
//     with this license.
//     (E) The software is licensed "as-is." You bear the risk of
//     using it. The contributors give no express warranties,
//     guarantees or conditions. You may have additional consumer
//     rights under your local laws which this license cannot change.
//     To the extent permitted under your local laws, the contributors
//     exclude the implied warranties of merchantability, fitness for
//     a particular purpose and non-infringement.
//

#if defined( _WIN32)
#include <windows.h>
#endif

#include <osd/cpuComputeContext.h>
#include <osd/cpuComputeController.h>
#include <osd/cpuEvalLimitContext.h>
#include <osd/cpuEvalLimitController.h>
#include <osd/cpuVertexBuffer.h>
#include <osd/error.h>
#include <osd/mesh.h>
#include <osd/vertex.h>

#include <osdutil/uniformEvaluator.h>
#include <osdutil/topology.h>

#include "../common/stopwatch.h"
#include "../../regression/common/shape_utils.h"


#include <cfloat>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>

#ifdef OPENSUBDIV_HAS_OPENMP
    #include <omp.h>
#endif

using namespace OpenSubdiv;

static shape * readShape( char const * fname ) {

    FILE * handle = fopen( fname, "rt" );
    if (not handle) {
        printf("Could not open \"%s\" - aborting.\n", fname);
        exit(0);
    }

    fseek( handle, 0, SEEK_END );
    size_t size = ftell(handle);
    fseek( handle, 0, SEEK_SET );

    char * shapeStr = new char[size+1];

    if ( fread( shapeStr, size, 1, handle)!=1 ) {
        printf("Error reading \"%s\" - aborting.\n", fname);
        exit(0);
    }

    fclose(handle);

    shapeStr[size]='\0';

    return shape::parseShape( shapeStr, 1 );
}

static bool
shapeToTopology( const shape *input, PxOsdUtilSubdivTopology *output,
                 std::string *errorMessage)
{
    int numVertices = input->getNverts();
    
    output->numVertices = numVertices;
    output->maxLevels = 3;  //arbitrary initial value
    output->nverts = input->nvertsPerFace;
    output->indices = input->faceverts;

    // XXX:gelder
    // Need to pull over uvs and tags for better test coverage

    return output->IsValid(errorMessage);
}


//------------------------------------------------------------------------------
static bool
createOsdMesh(char *inputFile, char *outputFile, std::string *errorMessage)
{

    shape *inputShape = readShape(inputFile);
    
    PxOsdUtilSubdivTopology topology;
    if (not shapeToTopology(inputShape, &topology, errorMessage))
        return false;

    PxOsdUtilUniformEvaluator uniformEvaluator;

    // Create uniformEvaluator
    if (not uniformEvaluator.Initialize(topology, errorMessage)) {
        return false;
    }

    // Push the vertex data
    uniformEvaluator.SetCoarsePositions(inputShape->verts, errorMessage);

    // Refine with eight threads
    if (not uniformEvaluator.Refine(8, errorMessage))
        return false;
    
    std::vector<int> refinedQuads;
    if (not uniformEvaluator.GetRefinedQuads(&refinedQuads, errorMessage)) {
        std::cout  << "GetRefinedQuads failed with " << errorMessage << std::endl;        
    }

    float *refinedPositions = NULL;
    int numFloats = 0;
    if (not uniformEvaluator.GetRefinedPositions(&refinedPositions, &numFloats, errorMessage)) {
        std::cout  << "GetRefinedPositions failed with " << errorMessage << std::endl;        
    }
    
    std::cout << "Quads = " << refinedQuads.size()/4 << std::endl;        
    for (int i=0; i<(int)refinedQuads.size(); i+=4)  {
        std::cout << "(" << refinedQuads[i] <<
            ", " << refinedQuads[i+1] <<
            ", " << refinedQuads[i+2] <<
            ", " << refinedQuads[i+3] <<
            ")\n";
    }
        
    std::cout << "Hot damn, it worked.\n";
    std::cout << "Positions = " << numFloats/3 << std::endl;
    for (int i=0; i<numFloats; i+=3)  {
        std::cout << "(" << refinedPositions[i] <<
            ", " << refinedPositions[i+1] <<
            "," << refinedPositions[i+2] << ")\n";
    }



//    if (not uniformEvaluator.WriteRefinedObj("foo.obj", errorMessage)) {
//        std::cout << errorMessage << std::endl;             
//    }


    return true;
}

//------------------------------------------------------------------------------
static void
callbackError(OpenSubdiv::OsdErrorType err, const char *message)
{
    printf("OsdError: %d\n", err);
    printf("%s", message);
}


//------------------------------------------------------------------------------
int main(int argc, char** argv) {


    if (argc < 3) {
        std::cout << "Usage: projectTest input.obj output\n";
        return false;
    }

    std::cout << "input is " << argv[1] << " and output is " << argv[2] <<std::endl;


    OsdSetErrorCallback(callbackError);

    std::string errorMessage;

    if (not createOsdMesh(argv[1], argv[2], &errorMessage)) {
        std::cout << "Failed with error: " << errorMessage << std::endl;
    }

}
