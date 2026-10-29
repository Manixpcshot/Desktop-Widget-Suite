#include "common/Animation.h"
#include "update/UpdatePolicy.h"
#include <iostream>
#include <stdexcept>
void check(bool ok) { if (!ok) throw std::runtime_error("check failed"); }
int main() {
    using namespace suite;
    for (double frequency : {5., 15., 30.}) {
        Spring s;
        for (int i=0;i<1200;++i) s.advance(1, 1./120, frequency);
        check(s.settled(1));
        for (int i=0;i<1200;++i) s.advance(0, 1./120, frequency);
        check(s.settled(0));
    }
    check(newer("v0.1.1","0.1.0"));
    check(newer("v1.0.0","0.99.99"));
    check(!newer("v0.1.0","0.1.1"));
    check(!newer("v0.1.1","0.1.1"));
    for(auto invalid:{"v1.2", "v1.2.3.4", "v1.2.3-beta", "v01.2.3", "v-1.2.3", "v999999999999999.0.0", "1.2."}) check(!version(invalid));
    check(validDigest("sha256:"+std::string(64,'a')));
    check(!validDigest("sha256:"+std::string(64,'z')));
    check(!validDigest("sha256:abc"));
    Spring bounce; bool overshot=false;
    for(int i=0;i<240;++i) { bounce.advance(1,1./120); overshot |= bounce.value>1; }
    check(overshot);
    Spring s; s.advance(1, 100); check(std::isfinite(s.value));
    const double old=s.value; s.advance(1, -1); check(old == s.value);
    Rect r{-1920, -200, 0, 880};
    check(atEdge(-1920, 0, r, false, 3));
    check(!atEdge(-1916, 0, r, false, 3));
    check(atEdge(-1, 0, r, true, 3));
    check(!atEdge(0, 0, r, true, 3));
    check(!atEdge(-1, 880, r, true, 3));
    check(stagger(0, 0)==0 && stagger(1, 4)==1);
    check(stagger(.5, 0)>stagger(.5, 4));
    std::cout << "Animation and multi-monitor edge tests passed\n";
}
