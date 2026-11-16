#pragma once
#include "naru/project/Project.h"
namespace naru {
inline const AnimationFrame* animationFrame(const SceneObject& object) {
    const auto* clip=findAnimation(object.animations,object.animationState.clip);
    if(!clip && object.automaticAnimation)clip=findAnimation(object.animations,"idle",object.animationState.facing);
    if(!clip||clip->frames.empty())return nullptr;
    return &clip->frames[std::min(object.animationState.frame,clip->frames.size()-1)];
}
inline SceneObject animationVisual(const SceneObject& object) {
    SceneObject result;result.x=object.x;result.y=object.y;result.width=object.width;result.height=object.height;
    result.image=object.image;result.visible=object.visible;result.color=object.color;
    if(const auto* frame=animationFrame(object)){
        auto& image=result.image;image.asset=frame->asset;
        if(image.width==0)image.width=frame->width;
        if(image.height==0)image.height=frame->height;
        image.offsetX+=object.width/2;image.offsetY+=object.height;
        image.anchorX=frame->anchorX;image.anchorY=frame->anchorY;
    }return result;
}
}
