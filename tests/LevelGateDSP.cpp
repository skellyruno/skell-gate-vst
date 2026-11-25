#include "dsp/shared/LiveLevelGate.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using skellcore::experimental::dynamics::LiveLevelGate;
static void check(bool x,const char* message){if(!x){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
 for(double fs:{44100.,48000.,96000.,192000.}){
  LiveLevelGate g;g.prepare(fs);for(int i=0;i<1000;++i)check(g.next(.001,-.001)==1.,"default unity");
  g.setParameters(1.,-45.,4.);double low=1.,high=0.,previous=1.,maxJump=0.;
  for(int i=0;i<int(fs*.5);++i){low=g.next(.001,-.001);maxJump=std::max(maxJump,std::abs(low-previous));previous=low;}
  check(low<.002,"quiet attenuation");
  for(int i=0;i<int(fs*.1);++i)high=g.next(.2,-.2);
  check(high>.999,"opens on active key");
  for(int i=0;i<int(fs*1.);++i)low=g.next(.001,-.001);
  check(low<.01,"release returns to quiet attenuation");
  g.setParameters(0.,-45.,4.);previous=low;
  for(int i=0;i<int(fs*.02);++i){double gain=g.next(.001,-.001);check(gain>=previous-1e-8&&gain<=1.,"disable ramp");maxJump=std::max(maxJump,std::abs(gain-previous));previous=gain;}
  check(previous==1.,"exact disabled unity");check(maxJump<.01,"mix automation bounded step");
  g.setParameters(1.,-45.,0.);for(int i=0;i<int(fs*.1);++i)check(g.next(.001,.001)==1.,"neutral expansion");
  g.setParameters(1.,-45.,4.);g.reset();LiveLevelGate ref;ref.prepare(fs);ref.setParameters(1.,-45.,4.);
  for(int i=0;i<1000;++i)check(g.next(.01,-.02)==ref.next(-.02,.01),"reset and channel symmetry");
  g.setParameters(1.,-85.,12.);for(int i=0;i<1000;++i){double y=g.next(std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity());check(std::isfinite(y)&&y>=0.&&y<=1.,"nonfinite control/input safety");}
  std::printf("%.0f Hz quiet gain %.9g active gain %.9g max mix step %.9g PASS\n",fs,low,high,maxJump);
 }
}
