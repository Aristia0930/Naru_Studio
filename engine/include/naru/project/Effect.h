#pragma once
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace naru {
struct EffectFrame {
    std::string image;
    int x=0,y=0,width=32,height=32;
    double seconds=.1,offsetX=0,offsetY=0,scale=1,rotation=0,opacity=1;
    std::string blend="normal", sound;
    bool targetFlash=false,screenFlash=false,screenShake=false;
    bool operator==(const EffectFrame&) const = default;
};
struct Effect {
    std::string id,name;
    std::vector<EffectFrame> frames;
    bool operator==(const Effect&) const = default;
};
inline void validateEffects(const std::vector<Effect>& effects) {
    if(effects.size()>256)throw std::invalid_argument("이펙트는 최대 256개입니다");
    std::set<std::string> ids;
    for(const auto& effect:effects){
        if(effect.id.empty()||effect.id.size()>64||!ids.insert(effect.id).second||effect.name.empty()||effect.frames.empty()||effect.frames.size()>4096)
            throw std::invalid_argument("이펙트 ID·이름·프레임 수 오류");
        for(unsigned char ch:effect.id)if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-'))
            throw std::invalid_argument("이펙트 ID는 영문·숫자·_·-만 사용합니다");
        for(const auto& f:effect.frames)if(f.image.empty()||f.x<0||f.y<0||f.width<1||f.height<1||f.width>16384||f.height>16384||!std::isfinite(f.seconds)||f.seconds<.01||f.seconds>60||!std::isfinite(f.offsetX)||!std::isfinite(f.offsetY)||!std::isfinite(f.scale)||f.scale<=0||f.scale>100||!std::isfinite(f.rotation)||!std::isfinite(f.opacity)||f.opacity<0||f.opacity>1||(f.blend!="normal"&&f.blend!="add"))
            throw std::invalid_argument("이펙트 프레임 이미지·영역·시간·표시 설정 오류");
    }
}
}
