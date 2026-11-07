#include "runtime/TerrainMesh.h"
#include "runtime/TerraformField.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

using Spore::Terrain::TerraformField;
using Spore::Terrain::WriteTerrainOBJ;

namespace {
void Check(bool v,const char* msg){ if(!v) throw std::runtime_error(msg); }
std::size_t LinesWith(const std::string& s,const std::string& prefix) {
  std::size_t result=0,begin=0;
  while(begin<s.size()){
    const auto end=s.find('\n',begin);
    if(s.compare(begin,prefix.size(),prefix)==0) ++result;
    if(end==std::string::npos) break;
    begin=end+1;
  }
  return result;
}
template<typename F> void Reject(F fn,const char* msg) {
  bool threw=false;
  try{fn();}catch(const std::invalid_argument&){threw=true;}
  Check(threw,msg);
}
void TestMesh(){
  TerraformField field(3,2,.5f);
  std::ostringstream out;
  const auto info=WriteTerrainOBJ(field,out,6.f,4.f);
  const auto text=out.str();
  Check(info.vertices==6&&info.triangles==4,"3x2 vertex and triangle count");
  Check(info.minimumHeight==.5f&&info.maximumHeight==.5f,"height statistics");
  Check(LinesWith(text,"v ")==6,"vertex records");
  Check(LinesWith(text,"vt ")==6,"uv records");
  Check(LinesWith(text,"vn ")==6,"normal records");
  Check(LinesWith(text,"f ")==4,"face records");
  Check(text.find("v -3 2 -3\n")!=std::string::npos,"origin XY Z");
  Check(text.find("vn 0 1 0\n")!=std::string::npos,"flat surface normals up");
  Check(text.find("f 1/1/1 4/4/4 2/2/2\n")!=std::string::npos,
        "triangle orientation positive Y");
  std::ostringstream duplicate;
  WriteTerrainOBJ(field,duplicate,6.f,4.f);
  Check(duplicate.str()==text,"export deterministic across runs");
  Reject([&]{WriteTerrainOBJ(field,out,0.f,4.f);},"reject invalid horizontal scale");
  Reject([&]{WriteTerrainOBJ(field,out,1.f,-1.f);},"reject invalid vertical scale");
  Reject([&]{WriteTerrainOBJ(field,out,std::numeric_limits<float>::quiet_NaN(),1.f);},
         "reject NaN");
  field.Apply(.5f,.5f,.8f,.35f);
  std::ostringstream modified;
  WriteTerrainOBJ(field,modified,6.f,4.f);
  Check(modified.str()!=text,"terrain changes mesh");
  // Every normal must be a unit-length vector with positive Y.
  std::istringstream in(modified.str());
  std::string tag;
  while(in>>tag){
    if(tag=="vn"){
      double x,y,z; in>>x>>y>>z;
      Check(y>0&&std::abs(x*x+y*y+z*z-1)<.0001,"normal unit length and positive Y");
    }
    std::string discard;
    std::getline(in,discard);
  }
}
void TestLimits(){
  TerraformField huge(513,513,.5f);
  std::ostringstream out;
  Reject([&]{WriteTerrainOBJ(huge,out);},"reject excessive output");
  Check(out.str().empty(),"invalid mesh writes nothing");
  TerraformField allowed(2,2,0.f);
  const auto m=WriteTerrainOBJ(allowed,out,1.f,0.f);
  Check(m.vertices==4&&m.triangles==2,"zero-height meshes work");
}
}
int main(){
  try{TestMesh();TestLimits();std::cout<<"Spore headless terrain mesh tests passed\n";return 0;}
  catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
