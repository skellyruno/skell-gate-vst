#pragma once
#include <algorithm>
#include <cmath>

namespace skellcore::experimental::dynamics {
// Independent dual-power expander: live adaptation of the owned research gate.
// All control updates are on the processing thread; prepare is off-thread.
// No delay, allocation, locking or IO; default disabled is exact unity.
class LiveLevelGate {
    double fs=48000., fast=0., slow=0., fastStep=0., slowStep=0., gainStep=0.;
    double threshold=-45., thresholdPower=0.00003162277660168379, exponent=2.;
    double mix=0., targetMix=0., mixStep=1./480., smoothedGain=1.;
    static double finiteClamp(double x,double lo,double hi,double fallback) noexcept {
        return std::isfinite(x)?std::clamp(x,lo,hi):fallback;
    }
    static double magnitude(double x) noexcept {
        return std::isfinite(x)?std::min(std::abs(x),64.):0.;
    }
public:
    LiveLevelGate() noexcept { prepare(48000.); }
    void prepare(double rate) noexcept {
        fs=finiteClamp(rate,8000.,384000.,48000.);
        fastStep=-std::expm1(-1000./(fs*2.));
        slowStep=-std::expm1(-1000./(fs*60.));
        gainStep=-std::expm1(-1000./fs);
        mixStep=100./fs;
        reset();
    }
    void reset() noexcept { fast=slow=mix=0.; smoothedGain=1.; }
    void setParameters(double amount,double thresholdDb,double expansion) noexcept {
        targetMix=finiteClamp(amount,0.,1.,0.);
        const double t=finiteClamp(thresholdDb,-85.,-15.,-45.);
        if(t!=threshold){threshold=t;thresholdPower=std::pow(10.,t/10.);}
        exponent=.5*finiteClamp(expansion,0.,12.,4.);
    }
    // Call once per stereo frame (duplicate mono input). Returned gain applies
    // equally to both channels. With amount zero, the effect is bypassed exactly.
    double next(double left,double right) noexcept {
        if(mix==0. && targetMix==0.) return 1.;
        mix=targetMix>mix?std::min(targetMix,mix+mixStep):std::max(targetMix,mix-mixStep);
        if(mix==0. && targetMix==0.) { reset(); return 1.; }
        const double p=std::max(magnitude(left),magnitude(right)),power=p*p;
        fast+=fastStep*(power-fast);slow+=slowStep*(power-slow);
        if(fast<1e-24)fast=0.;
        if(slow<1e-24)slow=0.;
        const double env=std::max(fast,slow);
        double target=1.;
        if(exponent>0. && env<thresholdPower)
            target=std::max(0.00003162277660168379,std::pow(env/thresholdPower,exponent));
        smoothedGain+=gainStep*(target-smoothedGain);
        if(std::abs(smoothedGain-target)<1e-12)smoothedGain=target;
        return std::clamp(1.+mix*(smoothedGain-1.),0.,1.);
    }
};
}
