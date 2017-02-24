// The main ray tracer.

#pragma warning (disable: 4786)

#include "RayTracer.h"
#include "scene/light.h"
#include "scene/material.h"
#include "scene/ray.h"

#include "parser/Tokenizer.h"
#include "parser/Parser.h"

#include "ui/TraceUI.h"
#include <cmath>
#include <algorithm>

extern TraceUI* traceUI;

#include <iostream>
#include <fstream>

using namespace std;

// Use this variable to decide if you want to print out
// debugging messages.  Gets set in the "trace single ray" mode
// in TraceGLWindow, for example.
bool debugMode = false;

// Trace a top-level ray through pixel(i,j), i.e. normalized window coordinates (x,y),
// through the projection plane, and out into the scene.  All we do is
// enter the main ray-tracing method, getting things started by plugging
// in an initial ray weight of (0.0,0.0,0.0) and an initial recursion depth of 0.

Vec3d RayTracer::trace(double x, double y)
{
  // Clear out the ray cache in the scene for debugging purposes,
  if (TraceUI::m_debug) scene->intersectCache.clear();
  ray r(Vec3d(0,0,0), Vec3d(0,0,0), ray::VISIBILITY);
  scene->getCamera().rayThrough(x,y,r);
  Vec3d ret = traceRay(r, traceUI->getDepth());
  ret.clamp();
  return ret;
}

Vec3d RayTracer::tracePixel(int i, int j)
{
	Vec3d col(0,0,0);

	if( ! sceneLoaded() ) return col;

	double x = double(i)/double(buffer_width);
	double y = double(j)/double(buffer_height);

	int aaSize = traceUI->getAASize();
	//anti-alias
	if(aaSize > 1) {
		//find new increment
		double xIncr = 1.0 / (double(buffer_width) * aaSize);
		double yIncr = 1.0 / (double(buffer_height) * aaSize);

		for(int xOffset = 0; xOffset < aaSize; ++xOffset) {
			for(int yOffset = 0; yOffset < aaSize; ++yOffset) {
				col += trace(x + xOffset * xIncr, y + yOffset * yIncr);
			}
		}
		col /= (aaSize * aaSize);

	} else col = trace(x, y);


	unsigned char *pixel = buffer + ( i + j * buffer_width ) * 3;

	pixel[0] = (int)( 255.0 * col[0]);
	pixel[1] = (int)( 255.0 * col[1]);
	pixel[2] = (int)( 255.0 * col[2]);
	return col;
}


// Do recursive ray tracing!  You'll want to insert a lot of code here
// (or places called from here) to handle reflection, refraction, etc etc.

bool TIR(Vec3d& N, Vec3d d, double index) {
	double angle = N * d;
	if(angle > 0.0) {
		if(1.0 - (index * index * (1.0 - angle * angle)) < 0.0) {
			return true;
		}
	}
	return false;
}

Vec3d RayTracer::traceRay(ray& r, int depth)
{
	isect i;
	Vec3d colorC;
	if(scene->intersect(r, i)) {
		const Material& m = i.getMaterial();
		colorC = m.shade(scene, r, i);
		if(depth < 1) {
			return colorC;
		}
		Vec3d iC = i.N * (-r.getDirection() * i.N);
		Vec3d iS = iC + r.getDirection();
		if(m.Refl()) {
		  Vec3d R = iC + iS;
		  R.normalize();
		  ray newRay = ray(r.at(i.t), R, ray::REFLECTION);
		  colorC += m.kr(i) % traceRay(newRay, depth - 1);
		}
		
		double n_i, n_t;
		Vec3d N;
		if(i.N * -r.getDirection() < 0) {
				//exiting
				N = i.N;
				n_i = m.index(i);
				n_t = 1.0;
		} else {
				//entering
				N = -i.N;
				n_i = 1.0;
				n_t = m.index(i);
		}
		
		if(m.Trans() && !TIR(i.N, r.getDirection(), m.index(i))) {
			Vec3d tS = (n_i / n_t) * iS;
			Vec3d tC;
			if(tS * tS > 1.0) tC = N;
			else tC = N * sqrt((1 - tS * tS));
				Vec3d T = tC + tS;
				T.normalize();
				ray newRay = ray(r.at(i.t), T, ray::REFRACTION);
				colorC += m.kt(i) % traceRay(newRay, depth - 1);
		}

	} else {
		// No intersection.  This ray travels to infinity, so we color
		// it according to the background color, which in this (simple) case
		// is just black.
		if(traceUI->gotCubeMap() && traceUI->useCubeMap()) {
			return cubemap->getColor(r);
			//return colorC = Vec3d(0, 0, 1);
		} else colorC = Vec3d(0, 0, 0);
	}
	return colorC;
}

RayTracer::RayTracer()
	: scene(0), buffer(0), buffer_width(256), buffer_height(256), m_bBufferReady(false), cubemap(NULL)
{}

RayTracer::~RayTracer()
{
	delete scene;
	delete [] buffer;
	delete cubemap;
}

void RayTracer::getBuffer( unsigned char *&buf, int &w, int &h )
{
	buf = buffer;
	w = buffer_width;
	h = buffer_height;
}

double RayTracer::aspectRatio()
{
	return sceneLoaded() ? scene->getCamera().getAspectRatio() : 1;
}

bool RayTracer::loadScene( char* fn ) {
	ifstream ifs( fn );
	if( !ifs ) {
		string msg( "Error: couldn't read scene file " );
		msg.append( fn );
		traceUI->alert( msg );
		return false;
	}
	
	// Strip off filename, leaving only the path:
	string path( fn );
	if( path.find_last_of( "\\/" ) == string::npos ) path = ".";
	else path = path.substr(0, path.find_last_of( "\\/" ));

	// Call this with 'true' for debug output from the tokenizer
	Tokenizer tokenizer( ifs, false );
    Parser parser( tokenizer, path );
	try {
		delete scene;
		scene = 0;
		scene = parser.parseScene();
	} 
	catch( SyntaxErrorException& pe ) {
		traceUI->alert( pe.formattedMessage() );
		return false;
	}
	catch( ParserException& pe ) {
		string msg( "Parser: fatal exception " );
		msg.append( pe.message() );
		traceUI->alert( msg );
		return false;
	}
	catch( TextureMapException e ) {
		string msg( "Texture mapping exception: " );
		msg.append( e.message() );
		traceUI->alert( msg );
		return false;
	}

	if( !sceneLoaded() ) return false;
	scene->buildKdTree();

	return true;
}

void RayTracer::traceSetup(int w, int h)
{
	if (buffer_width != w || buffer_height != h)
	{
		buffer_width = w;
		buffer_height = h;
		bufferSize = buffer_width * buffer_height * 3;
		delete[] buffer;
		buffer = new unsigned char[bufferSize];
	}
	memset(buffer, 0, w*h*3);
	m_bBufferReady = true;
}

