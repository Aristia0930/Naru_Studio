#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <cmath>
#include <set>
#include <stdexcept>

namespace naru {
struct AnimationFrame {
    std::string asset;
    int x=0,y=0,width=32,height=32;
    double seconds=.1, anchorX=.5, anchorY=1;
    std::string signal;
    bool operator==(const AnimationFrame&) const = default;
};
struct AnimationClip {
    std::string name="idle";
    bool loop=true;
    std::vector<AnimationFrame> frames;
    bool operator==(const AnimationClip&) const = default;
};
struct AnimationState {
    std::string clip, facing="down";
    std::size_t frame=0;
    double elapsed=0;
    bool overrideClip=false,loop=false,lock=false,moved=false;
    std::uint64_t token=0;
    bool operator==(const AnimationState&) const = default;
};
inline const AnimationClip* findAnimation(const std::vector<AnimationClip>& clips,const std::string& name,const std::string& facing="") {
    if(!facing.empty())for(const auto& c:clips)if(c.name==name+"_"+facing)return &c;
    for(const auto& c:clips)if(c.name==name)return &c;
    return nullptr;
}
inline void validateAnimations(const std::vector<AnimationClip>& clips) {
    if(clips.size()>256)throw std::invalid_argument("객체의 애니메이션 클립은 최대 256개입니다");
    std::set<std::string> names;
    for(const auto& c:clips){
        if(c.name.empty()||c.name.size()>64||!names.insert(c.name).second||c.frames.empty()||c.frames.size()>4096)throw std::invalid_argument("클립 이름 중복/빈 이름 또는 프레임 수 오류 (1~4096)");
        for(unsigned char ch:c.name)if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-'))throw std::invalid_argument("클립 이름은 영문·숫자·_·-만 사용합니다");
        for(const auto& f:c.frames)if(f.asset.empty()||f.x<0||f.y<0||f.x>100000||f.y>100000||f.width<1||f.height<1||f.width>16384||f.height>16384||!std::isfinite(f.seconds)||f.seconds<.01||f.seconds>60||!std::isfinite(f.anchorX)||!std::isfinite(f.anchorY)||f.anchorX<0||f.anchorX>1||f.anchorY<0||f.anchorY>1||f.signal.size()>64)throw std::invalid_argument("애니메이션 프레임 이미지·영역·시간·기준점·신호 오류");
    }
}
}
