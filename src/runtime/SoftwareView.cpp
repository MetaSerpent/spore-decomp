#include "runtime/SoftwareView.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <vector>
#include <cstdint>

namespace Spore::Terrain::View {
namespace {
constexpr float kPi = 3.14159265358979323846f;
struct Pt { float x = 0, y = 0, depth = 0; };
struct Transform {
  float cy,sy,sp,cp,factor,cx,cyScreen;
};
bool IsValid(const Camera& cam) {
  return std::isfinite(cam.yaw) && std::isfinite(cam.pitch) &&
    std::isfinite(cam.zoom) && std::isfinite(cam.panX) &&
    std::isfinite(cam.panY) && cam.pitch >= .25f && cam.pitch <= 1.4f &&
    cam.zoom >= .25f && cam.zoom <= 8.f &&
    std::fabs(cam.panX) <= 20.f && std::fabs(cam.panY) <= 20.f;
}
Transform Projection(const Camera& cam, uint32_t w, uint32_t h) {
  return {std::cos(cam.yaw),std::sin(cam.yaw),
          std::sin(cam.pitch),std::cos(cam.pitch),
          float(std::min(w,h))*.40f*cam.zoom,
          float(w)*.5f,float(h)*.5f};
}
Pt Project(float x,float z,float height,const Camera& cam,
           const Transform& t) {
  const float wx=(x-.5f)*2.f, wz=(z-.5f)*2.f;
  const float axis=t.cy*wx-t.sy*wz;
  const float transverse=t.sy*wx+t.cy*wz;
  const float y=height*.65f;
  const float screenV=t.cp*y-t.sp*transverse;
  return {t.cx+(axis-cam.panX)*t.factor,
          t.cyScreen-(screenV-cam.panY)*t.factor,
          t.sp*y+t.cp*transverse};
}
float Edge(const Pt& a,const Pt& b,float x,float y) {
  return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);
}
uint32_t Palette(float height,float shade) {
  const float h=std::clamp(height,0.f,1.f);
  float red,green,blue;
  if(h<.30f){
    const float t=h/.30f;
    red=22.f+24.f*t;green=46.f+82.f*t;blue=88.f+12.f*t;
  }else if(h<.74f){
    const float t=(h-.30f)/.44f;
    red=47.f+102.f*t;green=128.f+13.f*t;blue=100.f-46.f*t;
  }else{
    const float t=(h-.74f)/.26f;
    red=149.f+86.f*t;green=141.f+102.f*t;blue=54.f+183.f*t;
  }
  const auto component=[shade](float x)->uint32_t{
    return static_cast<uint32_t>(std::clamp(x*shade,0.f,255.f));
  };
  return (component(red)<<16)|(component(green)<<8)|component(blue);
}
void FillTriangle(Frame& output,std::vector<float>& depths,
                  const Pt& a,const Pt& b,const Pt& c,uint32_t colour) {
  const float area=Edge(a,b,c.x,c.y);
  if(!std::isfinite(area)||std::fabs(area)<.00001f)return;
  const int minX=std::max(0,int(std::floor(std::min({a.x,b.x,c.x}))));
  const int maxX=std::min(int(output.width)-1,int(std::ceil(std::max({a.x,b.x,c.x}))));
  const int minY=std::max(0,int(std::floor(std::min({a.y,b.y,c.y}))));
  const int maxY=std::min(int(output.height)-1,int(std::ceil(std::max({a.y,b.y,c.y}))));
  for(int y=minY;y<=maxY;++y)for(int x=minX;x<=maxX;++x){
    const float sx=float(x)+.5f,sy=float(y)+.5f;
    const float wa=Edge(b,c,sx,sy)/area;
    const float wb=Edge(c,a,sx,sy)/area;
    const float wc=1.f-wa-wb;
    if(wa<-.00001f||wb<-.00001f||wc<-.00001f)continue;
    const float depth=wa*a.depth+wb*b.depth+wc*c.depth;
    const std::size_t i=std::size_t(y)*output.width+std::size_t(x);
    if(depth>depths[i]){depths[i]=depth;output.rgb[i]=colour;}
  }
}
} // namespace

void ApplyCameraAction(Camera& camera,Action action,float strength) {
  if(!IsValid(camera)||!std::isfinite(strength)||strength<0||strength>100)
    throw std::invalid_argument("invalid camera or action strength");
  if(action==Action::Reset){camera=Camera{};return;}
  switch(action){
    case Action::Left: camera.panX-=.08f*strength;break;
    case Action::Right: camera.panX+=.08f*strength;break;
    case Action::Up: camera.panY+=.08f*strength;break;
    case Action::Down: camera.panY-=.08f*strength;break;
    case Action::RotateLeft: camera.yaw-=.10f*strength;break;
    case Action::RotateRight: camera.yaw+=.10f*strength;break;
    case Action::TiltUp: camera.pitch=std::min(1.4f,camera.pitch+.06f*strength);break;
    case Action::TiltDown: camera.pitch=std::max(.25f,camera.pitch-.06f*strength);break;
    case Action::ZoomIn: camera.zoom=std::min(8.f,camera.zoom*(1.f+.10f*strength));break;
    case Action::ZoomOut: camera.zoom=std::max(.25f,camera.zoom/(1.f+.10f*strength));break;
    case Action::Reset: break;
  }
  camera.panX=std::clamp(camera.panX,-20.f,20.f);
  camera.panY=std::clamp(camera.panY,-20.f,20.f);
  if(std::fabs(camera.yaw)>2*kPi)camera.yaw=std::remainder(camera.yaw,2*kPi);
}

Frame RenderTerrain(const TerraformField& terrain,const Camera& camera,
                    uint32_t w,uint32_t h,const std::vector<Marker>& markers,
                    const std::vector<CreatureInstance>& creatures) {
  if(!IsValid(camera)||w<32||h<32||uint64_t(w)*h>4000000ULL||
     uint64_t(terrain.Width())*terrain.Height()>131072ULL ||
     markers.size()>10000 || creatures.size()>128)
    throw std::invalid_argument("invalid software-renderer dimensions or camera");
  for(const auto& marker:markers){
    if(!std::isfinite(marker.x)||!std::isfinite(marker.z)||
       !std::isfinite(marker.radius)||marker.radius<0||
       marker.radius>64)
      throw std::invalid_argument("invalid organism marker");
  }
  Frame f;f.width=w;f.height=h;
  f.rgb.resize(std::size_t(w)*h);
  std::vector<float> depth(f.rgb.size(),-std::numeric_limits<float>::infinity());
  for(uint32_t y=0;y<h;++y){
    const float t=float(y)/float(h);
    const auto r=uint32_t(14.f+24.f*t);
    const auto g=uint32_t(24.f+30.f*t);
    const auto b=uint32_t(37.f+30.f*t);
    std::fill(f.rgb.begin()+std::size_t(y)*w,
              f.rgb.begin()+std::size_t(y+1)*w,(r<<16)|(g<<8)|b);
  }
  const auto projection=Projection(camera,w,h);
  const auto tw=terrain.Width(),th=terrain.Height();
  std::vector<Pt> projected(std::size_t(tw)*th);
  for(uint32_t z=0;z<th;++z)for(uint32_t x=0;x<tw;++x){
    projected[std::size_t(z)*tw+x]=Project(
      float(x)/float(tw-1),float(z)/float(th-1),
      terrain.At(x,z),camera,projection);
  }
  for(uint32_t z=0;z+1<th;++z)for(uint32_t x=0;x+1<tw;++x){
    const auto i=std::size_t(z)*tw+x;
    const auto a=terrain.At(x,z),b=terrain.At(x+1,z),
               c=terrain.At(x,z+1),d=terrain.At(x+1,z+1);
    const float dx=(b+d-a-c)*.5f;
    const float dz=(c+d-a-b)*.5f;
    const auto shade=std::clamp(.87f-dx*2.1f+dz*1.3f,.50f,1.2f);
    const auto colour=Palette((a+b+c+d)*.25f,shade);
    FillTriangle(f,depth,projected[i],projected[i+tw],
                 projected[i+1],colour);
    FillTriangle(f,depth,projected[i+1],projected[i+tw],
                 projected[i+tw+1],colour);
  }
  // Actual indexed 3D anatomy (not just icon overlays). Creature shapes
  // are generated from traits and gait phase, transformed into the terrain
  // coordinate system, then depth-tested against world triangles.
  for(const auto& creature:creatures) {
    if(!std::isfinite(creature.x)||!std::isfinite(creature.z)||
       !std::isfinite(creature.yaw)||!std::isfinite(creature.phase)||
       creature.x<0||creature.x>1||creature.z<0||creature.z>1)
      throw std::invalid_argument("invalid creature transform");
    const auto anatomy=Spore::Creature::GenerateCreature(
        creature.traits,creature.phase,8,4);
    const auto baseX=uint32_t(std::lround(creature.x*float(tw-1)));
    const auto baseZ=uint32_t(std::lround(creature.z*float(th-1)));
    const auto baseY=terrain.At(baseX,baseZ);
    const auto cosine=std::cos(creature.yaw);
    const auto sine=std::sin(creature.yaw);
    std::vector<Pt> projectedCreature;
    projectedCreature.reserve(anatomy.vertices.size());
    for(const auto& v:anatomy.vertices){
      const float x=creature.x+
         (cosine*v.x-sine*v.z)/float(tw-1);
      const float z=creature.z+
         (sine*v.x+cosine*v.z)/float(th-1);
      projectedCreature.push_back(Project(
         x,z,baseY+v.y*.23f,camera,projection));
    }
    for(const auto& t:anatomy.triangles){
      // Height-dependent simple Lambert shading to expose creature relief.
      const auto& a=anatomy.vertices[t.a];
      const auto& b=anatomy.vertices[t.b];
      const auto& c=anatomy.vertices[t.c];
      const float ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z;
      const float vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
      const float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
      const float length=std::sqrt(nx*nx+ny*ny+nz*nz);
      if(length<1e-7f)continue;
      const float light=std::clamp(.72f+
         .23f*(nx*.2f+ny*.9f-nz*.35f)/length,.35f,1.f);
      const auto channel=[light](uint32_t v){
        return uint32_t(std::clamp(float(v)*light,0.f,255.f));
      };
      const auto colour=(channel((t.colour>>16)&255)<<16)|
                        (channel((t.colour>>8)&255)<<8)|
                        channel(t.colour&255);
      FillTriangle(f,depth,projectedCreature[t.a],
                   projectedCreature[t.b],projectedCreature[t.c],colour);
    }
  }
  for(const auto& marker:markers){
    if(marker.x<0||marker.x>1||marker.z<0||marker.z>1||marker.radius<=0)continue;
    const auto tx=uint32_t(std::lround(marker.x*float(tw-1)));
    const auto tz=uint32_t(std::lround(marker.z*float(th-1)));
    const auto p=Project(marker.x,marker.z,terrain.At(tx,tz)+.13f,
                         camera,projection);
    const int radius=int(std::ceil(marker.radius));
    const int lowX=std::max(0,int(p.x)-radius),highX=std::min(int(w)-1,int(p.x)+radius);
    const int lowY=std::max(0,int(p.y)-radius),highY=std::min(int(h)-1,int(p.y)+radius);
    for(int y=lowY;y<=highY;++y)for(int x=lowX;x<=highX;++x){
      const float dx=float(x)+.5f-p.x,dy=float(y)+.5f-p.y;
      if(dx*dx+dy*dy>marker.radius*marker.radius)continue;
      const auto i=std::size_t(y)*w+std::size_t(x);
      if(p.depth+.06f<depth[i])continue;
      f.rgb[i]=marker.colour&0x00FFFFFFU;
      depth[i]=p.depth+.06f;
    }
  }
  return f;
}

std::pair<float,float> ScreenToTerrain(const Camera& cam,uint32_t w,uint32_t h,
                                      float sx,float sy,float guessHeight) {
  if(!IsValid(cam)||w<32||h<32||
     !std::isfinite(sx)||!std::isfinite(sy)||
     !std::isfinite(guessHeight)||guessHeight<0||guessHeight>1)
    throw std::invalid_argument("invalid viewer picking parameters");
  const auto t=Projection(cam,w,h);
  const float horizontal=(sx-t.cx)/t.factor+cam.panX;
  const float vertical=-(sy-t.cyScreen)/t.factor+cam.panY;
  const float transverse=(t.cp*(guessHeight*.65f)-vertical)/t.sp;
  const float wx=t.cy*horizontal+t.sy*transverse;
  const float wz=-t.sy*horizontal+t.cy*transverse;
  return {wx*.5f+.5f,wz*.5f+.5f};
}

uint64_t Frame::Checksum() const noexcept {
  uint64_t h=UINT64_C(14695981039346656037);
  const auto hashWord=[&h](uint32_t v){
    for(int i=0;i<4;++i){h^=uint8_t(v>>(8*i));h*=UINT64_C(1099511628211);}
  };
  hashWord(width);hashWord(height);
  for(uint32_t pixel:rgb)hashWord(pixel);
  return h;
}
void WritePPM(const Frame& image,std::ostream& out) {
  if(!image.width||!image.height||
     image.rgb.size()!=uint64_t(image.width)*image.height)
    throw std::invalid_argument("invalid viewer frame");
  out<<"P6\n"<<image.width<<' '<<image.height<<"\n255\n";
  for(uint32_t p:image.rgb){
    const unsigned char c[3]={uint8_t(p>>16),uint8_t(p>>8),uint8_t(p)};
    out.write(reinterpret_cast<const char*>(c),3);
  }
  if(!out)throw std::runtime_error("viewer PPM stream failure");
}

} // namespace Spore::Terrain::View
