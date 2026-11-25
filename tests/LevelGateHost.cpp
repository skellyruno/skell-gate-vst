#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <cstdlib>
static void check(bool b,const char* msg){if(!b){std::cerr<<"LEVEL_GATE_HOST: "<<msg<<"\n";std::exit(1);}}
static void set(SkellGlitchAudioProcessor& p,const char* id,float v){auto* q=p.apvts.getParameter(id);check(q!=nullptr,"missing parameter");q->setValueNotifyingHost(q->convertTo0to1(v));}
int main(){
 juce::ScopedJuceInitialiser_GUI init;
 SkellGlitchAudioProcessor p;
 auto amount=[&]{return p.apvts.getRawParameterValue("levelGateAmount")->load();};
 check(amount()==0,"default must be legacy");
 p.loadFactoryPreset(28);check(amount()==100&&p.currentPresetName()=="Level Cleanup","new factory preset");
 auto saved=p.apvts.copyState();
 juce::MemoryBlock current;p.getStateInformation(current);
 auto legacy=saved.createCopy();
 for(const char* id:{"levelGateAmount","levelGateThreshold","levelGateExpansion"})
     legacy.removeChild(legacy.getChildWithProperty("id",id),nullptr);
 auto xml=legacy.createXml();check(xml!=nullptr,"legacy XML");
 juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*xml,old);
 p.setStateInformation(old.getData(),int(old.getSize()));
 check(amount()==0,"old project reset on same instance");
 p.snapshotAB(0);p.setStateInformation(current.getData(),int(current.getSize()));
 check(amount()==100,"new roundtrip");
 p.recallAB(0);check(amount()==0,"A/B recall");
 p.setStateInformation(current.getData(),int(current.getSize()));
 check(amount()==100,"new roundtrip after old A/B");
 // Compare the actual processor, full band and all pattern steps on,
 // equal input with independent reset. Measure tail windows after settling.
 auto run=[&](float a) {
   p.loadFactoryPreset(28);set(p,"levelGateAmount",a);p.prepareToPlay(48000,256);
   check(p.getLatencySamples()==0,"added latency");
   juce::AudioBuffer<float> b(2,256);juce::MidiBuffer midi;double q=0,loud=0;
   for(int block=0;block<250;++block){
     float amp=block<125?.001f:.2f;
     for(int i=0;i<256;++i){float x=amp*float(std::sin((block*256+i)*.1));b.setSample(0,i,x);b.setSample(1,i,-x);}
     p.processBlock(b,midi);
     for(int i=0;i<256;++i){float x=b.getSample(0,i),y=b.getSample(1,i);check(std::isfinite(x)&&std::abs(x+y)<1e-5f,"invalid/stereo");
       if(block>=100&&block<125)q+=double(x)*x;if(block>=225)loud+=double(x)*x;}
   }
   return std::array<double,2>{q,loud};
 };
 const auto a=run(0),b=run(100);
 check(a[0]>0&&b[0]<a[0]*.001,"quiet suppression");
 check(b[1]>a[1]*.99&&b[1]<a[1]*1.01,"active signal unity");
 for(int i=0;i<28;++i){p.loadFactoryPreset(i);check(amount()==0,"legacy factory changed");}
 std::cout<<"LEVEL_GATE_HOST: quiet energy ratio="<<b[0]/a[0]<<" loud ratio="<<b[1]/a[1]<<" state, 28 legacy factory defaults and zero latency PASS\n";
}
