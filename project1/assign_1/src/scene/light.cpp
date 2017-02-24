#include <cmath>

#include "light.h"

using namespace std;

double DirectionalLight::distanceAttenuation(const Vec3d& P) const
{
  // distance to light is infinite, so f(di) goes to 0.  Return 1.
  return 1.0;
}


Vec3d DirectionalLight::shadowAttenuation(const Scene* scene, const Vec3d& p) const
{
  Vec3d d = getDirection(p);
  isect i;
  ray lightRay = ray(p, d);
  if(scene->intersect(lightRay, i)) {
    const Material& m = i.getMaterial();
      //if(m.Trans()) return m.kt(i) / (m.kt(i) + m.kd(i));
      //#TODO
    if(m.Trans()) return m.kt(i);
    return Vec3d(0, 0, 0);
  } else return Vec3d(1, 1, 1);
}

Vec3d DirectionalLight::getColor() const
{
  return color;
}

Vec3d DirectionalLight::getDirection(const Vec3d& P) const
{
  // for directional light, direction doesn't depend on P
  return -orientation;
}

double PointLight::distanceAttenuation(const Vec3d& P) const
{
  double distance = (position - P).length();

  return min(1.0, 1.0 / (constantTerm + linearTerm * distance + quadraticTerm * pow(distance, 2)));

  // You'll need to modify this method to attenuate the intensity 
  // of the light based on the distance between the source and the 
  // point P.  For now, we assume no attenuation and just return 1.0
}

Vec3d PointLight::getColor() const
{
  return color;
}

Vec3d PointLight::getDirection(const Vec3d& P) const
{
  Vec3d ret = position - P;
  ret.normalize();
  return ret;
}


Vec3d PointLight::shadowAttenuation(const Scene* scene, const Vec3d& p) const
{
  Vec3d d = getDirection(p);
  isect i;
  ray lightRay = ray(p, d, ray::SHADOW);
  if(scene->intersect(lightRay, i)) {
    if(i.t < (position - p).length()) {
      const Material& m = i.getMaterial();
      //if(m.Trans()) return m.kt(i) / (m.kt(i) + m.kd(i));
      //#TODO
      //return Vec3d(0, 0, 0);
      if(m.Trans()) return m.kt(i);
      else return Vec3d(0, 0, 0);
    } else {
      return Vec3d(1, 1, 1);
    }
  } else return Vec3d(1, 1, 1);
}
