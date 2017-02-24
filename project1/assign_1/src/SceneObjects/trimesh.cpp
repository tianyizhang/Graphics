#include <cmath>
#include <float.h>
#include <algorithm>
#include <assert.h>
#include "trimesh.h"
#include "../ui/TraceUI.h"
extern TraceUI* traceUI;

using namespace std;

Trimesh::~Trimesh()
{
	for( Materials::iterator i = materials.begin(); i != materials.end(); ++i )
		delete *i;
    if(kdtree) delete kdtree;
}

// must add vertices, normals, and materials IN ORDER
void Trimesh::addVertex( const Vec3d &v )
{
    vertices.push_back( v );
}

void Trimesh::addMaterial( Material *m )
{
    materials.push_back( m );
}

void Trimesh::addNormal( const Vec3d &n )
{
    normals.push_back( n );
}

// Returns false if the vertices a,b,c don't all exist
bool Trimesh::addFace( int a, int b, int c )
{
    int vcnt = vertices.size();

    if( a >= vcnt || b >= vcnt || c >= vcnt ) return false;

    TrimeshFace *newFace = new TrimeshFace( scene, new Material(*this->material), this, a, b, c );
    newFace->setTransform(this->transform);
    if (!newFace->degen) faces.push_back( newFace );


    // Don't add faces to the scene's object list so we can cull by bounding box
    // scene->add(newFace);
    return true;
}

char* Trimesh::doubleCheck()
// Check to make sure that if we have per-vertex materials or normals
// they are the right number.
{
    if( !materials.empty() && materials.size() != vertices.size() )
        return "Bad Trimesh: Wrong number of materials.";
    if( !normals.empty() && normals.size() != vertices.size() )
        return "Bad Trimesh: Wrong number of normals.";

    return 0;
}

bool Trimesh::intersectLocal(ray& r, isect& i) const
{
	bool have_one = false;
    if(kdtree && traceUI->useKdTree()) {
        kdtree->intersect(r, i, have_one);
    } else {
        double tmin = 0.0;
        double tmax = 0.0;
        typedef Faces::const_iterator iter;
    	for( iter j = faces.begin(); j != faces.end(); ++j )
    	  {
    	    isect cur;
    	    if( (*j)->intersectLocal( r, cur ) )
    	      {
    		if( !have_one || (cur.t < i.t) )
    		  {
    		    i = cur;
    		    have_one = true;
    		  }
    	      }
    	  }
    }
	if( !have_one ) i.setT(1000.0);
	return have_one;
}

bool TrimeshFace::intersect(ray& r, isect& i) const {
  return intersectLocal(r, i);
}

// Intersect ray r with the triangle abc.  If it hits returns true,
// and put the parameter in t and the barycentric coordinates of the
// intersection in alpha, beta and gamma.

bool TrimeshFace::intersectLocal(ray& r, isect& i) const
{

    const Vec3d& a = parent->vertices[ids[0]];

    Vec3d rpv = r.p - a;
    double rd = n * r.d;
    //parallel or intersect?
    if(fabs(rd) < RAY_EPSILON) return false;

    double t = -(n * rpv) / (rd);
    if(t < RAY_EPSILON) return false;

    //intersection point
    Vec3d interP = r.p + t * r.d;
    Vec3d pa = interP - a;
    double pavab = pa * vab;
    double pavac = pa * vac;

    //in plane?
    double beta = (abac * pavac - acac * pavab) * triArea;
    if(beta < 0.0) return false;

    double gamma = (abac * pavab - abab * pavac) * triArea;
    if(gamma < 0.0 || gamma + beta > 1.0) return false;

    double alpha = 1 - beta - gamma;

    Vec3d baryCoord = Vec3d(alpha, beta, gamma);
    i.setObject(this);

    //material interpolation    
    if(!parent->materials.empty()) {
        Material m = alpha * (*parent->materials[ids[0]]);
        m += beta * (*parent->materials[ids[1]]);
        m += gamma * (*parent->materials[ids[2]]);
        i.setMaterial(m);
    } else i.setMaterial(this->getMaterial());

    i.setT(t);
    Vec3d N;

    //phong interpolation of normal
    if(!parent->normals.empty()) {
        Vec3d interpN1 = alpha * parent->normals[ids[0]];
        Vec3d interpN2 = beta * parent->normals[ids[1]];
        Vec3d interpN3 = gamma * parent->normals[ids[2]];
        N = interpN1 + interpN2 + interpN3;
        N.normalize();
    } else N = normal;

    i.setN(N);
    Vec2d uv = Vec2d(beta, gamma);
    i.setUVCoordinates(uv);
    i.setBary(baryCoord);
    return true;

}

void Trimesh::generateNormals()
// Once you've loaded all the verts and faces, we can generate per
// vertex normals by averaging the normals of the neighboring faces.
{
    int cnt = vertices.size();
    normals.resize( cnt );
    int *numFaces = new int[ cnt ]; // the number of faces assoc. with each vertex
    memset( numFaces, 0, sizeof(int)*cnt );
    
    for( Faces::iterator fi = faces.begin(); fi != faces.end(); ++fi )
    {
		Vec3d faceNormal = (**fi).getNormal();
        
        for( int i = 0; i < 3; ++i )
        {
            normals[(**fi)[i]] += faceNormal;
            ++numFaces[(**fi)[i]];
        }
    }

    for( int i = 0; i < cnt; ++i )
    {
        if( numFaces[i] )
            normals[i]  /= numFaces[i];
    }

    delete [] numFaces;
    vertNorms = true;
}
