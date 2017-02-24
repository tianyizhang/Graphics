#include "material.h"
#include "ray.h"
#include "light.h"
#include "../ui/TraceUI.h"
extern TraceUI* traceUI;

#include "../fileio/bitmap.h"
#include "../fileio/pngimage.h"

using namespace std;
extern bool debugMode;

// Apply the phong model to this point on the surface of the object, returning
// the color of that point.
Vec3d reflectDirectionM(const isect& i, const Light* l, const ray& r) {
    return (2.0 * (l->getDirection(r.at(i.t)) * i.N) * i.N - l->getDirection(r.at(i.t)));
}

Vec3d Material::shade(Scene *scene, const ray& r, const isect& i) const
{
  Vec3d colorC = ke(i) + ka(i) % scene->ambient();
  Vec3d view = scene->getCamera().getEye() - r.at(i.t);
  view.normalize();
  for ( vector<Light*>::const_iterator litr = scene->beginLights(); 
    litr != scene->endLights(); ++litr ) {
      Light* light = *litr;
      //if(i.N * light->getDirection(r.at(i.t)) < 0) continue;
      Vec3d lColor = light->getColor();
      Vec3d atten = light->distanceAttenuation(r.at(i.t)) * light->shadowAttenuation(scene, r.at(i.t));
      Vec3d diffuseTerm = kd(i) * max(i.N * light->getDirection(r.at(i.t)), 0.0);
      Vec3d R = reflectDirectionM(i, light, r);
      R.normalize();
      Vec3d specTerm = ks(i) * pow(max(0.0, (R * view)), shininess(i));
      colorC += atten % lColor % (diffuseTerm + specTerm);
  }
  return colorC;
}

TextureMap::TextureMap( string filename ) {

	int start = (int) filename.find_last_of('.');
	int end = (int) filename.size() - 1;
	if (start >= 0 && start < end) {
		string ext = filename.substr(start, end);
		if (!ext.compare(".png")) {
			png_cleanup(1);
			if (!png_init(filename.c_str(), width, height)) {
				double gamma = 2.2;
				int channels, rowBytes;
				unsigned char* indata = png_get_image(gamma, channels, rowBytes);
				int bufsize = rowBytes * height;
				data = new unsigned char[bufsize];
				for (int j = 0; j < height; j++)
					for (int i = 0; i < rowBytes; i += channels)
						for (int k = 0; k < channels; k++)
							*(data + k + i + j * rowBytes) = *(indata + k + i + (height - j - 1) * rowBytes);
				png_cleanup(1);
			}
		}
		else
			if (!ext.compare(".bmp")) data = readBMP(filename.c_str(), width, height);
			else data = NULL;
	} else data = NULL;
	if (data == NULL) {
		width = 0;
		height = 0;
		string error("Unable to load texture map '");
		error.append(filename);
		error.append("'.");
		throw TextureMapException(error);
	}
}

Vec3d TextureMap::getMappedValue( const Vec2d& coord ) const
{
  // YOUR CODE HERE

  // In order to add texture mapping support to the 
  // raytracer, you need to implement this function.
  // What this function should do is convert from
  // parametric space which is the unit square
  // [0, 1] x [0, 1] in 2-space to bitmap coordinates,
  // and use these to perform bilinear interpolation
  // of the values.

  //to texture coord

  double dCoordX = coord[0] * width;
  double dCoordY = coord[1] * height;

  int texCoordX = int(dCoordX);
  int texCoordY = int(dCoordY);

  //bilinear interpolation
  Vec3d color1 = getPixelAt(texCoordX, texCoordY);
  Vec3d color2 = getPixelAt(texCoordX + 1, texCoordY);
  Vec3d color3 = getPixelAt(texCoordX, texCoordY + 1);
  Vec3d color4 = getPixelAt(texCoordX + 1, texCoordY + 1);

  double dx = dCoordX - double(texCoordX);
  double dy = dCoordY - double(texCoordY);
  color1 *= (1.0 - dx) * (1.0 - dy);
  color2 *= dx * (1.0 - dy);
  color3 *= (1.0 - dx) * dy;
  color4 *= dx * dy;

  return color1 + color2 + color3 + color4;

}


Vec3d TextureMap::getPixelAt( int x, int y ) const
{
    // This keeps it from crashing if it can't load
    // the texture, but the person tries to render anyway.
    if (0 == data)
      return Vec3d(1.0, 1.0, 1.0);

    if( x >= width )
       x = width - 1;
    if( y >= height )
       y = height - 1;

    // Find the position in the big data array...
    int pos = (y * width + x) * 3;
    return Vec3d(double(data[pos]) / 255.0, 
       double(data[pos+1]) / 255.0,
       double(data[pos+2]) / 255.0);
}

Vec3d MaterialParameter::value( const isect& is ) const
{
    if( 0 != _textureMap )
        return _textureMap->getMappedValue( is.uvCoordinates );
    else
        return _value;
}

double MaterialParameter::intensityValue( const isect& is ) const
{
    if( 0 != _textureMap )
    {
        Vec3d value( _textureMap->getMappedValue( is.uvCoordinates ) );
        return (0.299 * value[0]) + (0.587 * value[1]) + (0.114 * value[2]);
    }
    else
        return (0.299 * _value[0]) + (0.587 * _value[1]) + (0.114 * _value[2]);
}

