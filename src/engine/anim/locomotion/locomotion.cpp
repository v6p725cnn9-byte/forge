#define GLM_ENABLE_EXPERIMENTAL
#include "engine/anim/locomotion/locomotion.hpp"

#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace forge::anim {
namespace {
float smooth(float a, float b, float t) { return a + (b-a)*t; }
float weight(float value, float lo, float hi) { return std::clamp((value-lo)/(hi-lo), 0.0f, 1.0f); }
int node(const assets::Scene& scene, const std::string& name)
{
    for (std::size_t i=0; i<scene.nodes.size(); ++i) {
        const auto& n = scene.nodes[i].name;
        if (n == name || n == "mixamorig:" + name || n == "mixamorig" + name) return static_cast<int>(i);
    }
    return -1;
}
int clip(const assets::Scene& scene, const char* name)
{
    for (std::size_t i=0; i<scene.animations.size(); ++i)
        if (scene.animations[i].name == name) return static_cast<int>(i);
    return -1;
}
void blend_pose(std::vector<glm::vec3>& t, std::vector<glm::quat>& r, std::vector<glm::vec3>& s,
                const std::vector<glm::vec3>& bt, const std::vector<glm::quat>& br,
                const std::vector<glm::vec3>& bs, float alpha)
{
    for (std::size_t i=0; i<t.size(); ++i) {
        t[i] = glm::mix(t[i], bt[i], alpha);
        r[i] = glm::normalize(glm::slerp(r[i], br[i], alpha));
        s[i] = glm::mix(s[i], bs[i], alpha);
    }
}
}

const char* movement_name(MoveState state)
{
    static constexpr const char* names[]{"idle","start","walk","run","sprint","stop","pivot","turn",
        "jump","fall","land","recover","crouch","crouch walk","crawl","mantle","reach","push","pull"};
    return names[static_cast<int>(state)];
}

void Locomotion::bind(const assets::Scene& scene, float model_scale)
{
    scale_ = model_scale;
    idle_=clip(scene,"idle"); walk_=clip(scene,"walk"); run_=clip(scene,"run");
    hip_=node(scene,"Hips"); spine_=node(scene,"Spine"); chest_=node(scene,"Spine2"); head_=node(scene,"Head");
    for (int i=0;i<2;++i) {
        const std::string side=i==0?"Left":"Right";
        thigh_[i]=node(scene,side+"UpLeg"); shin_[i]=node(scene,side+"Leg"); foot_[i]=node(scene,side+"Foot");
        arm_[i]=node(scene,side+"Arm"); forearm_[i]=node(scene,side+"ForeArm");
    }
    if (hip_ >= 0) {
        std::vector<glm::vec3> t,s; std::vector<glm::quat> r; std::vector<glm::mat4> g;
        sample_clip(scene,idle_,0,t,r,s); hip_origin_=t[hip_];
        compute_globals(scene,t,r,s,g);
        const int parent=scene.nodes[hip_].parent;
        units_=1.0f/(scale_* (parent<0 ? 1.0f : glm::length(glm::vec3(g[parent][1]))));
        const float walk = stride_speed(scene,hip_,walk_)*scale_;
        const float run = stride_speed(scene,hip_,run_)*scale_;
        if (walk>.2f) walk_stride_=walk;
        if (run>.2f) run_stride_=run;
    }
}

void Locomotion::evaluate(const assets::Scene& scene, LocomotionState& v, const Motion& m,
                         float yaw, float dt, Palette& palette) const
{
    if (hip_<0 || scene.skins.empty()) { anim::evaluate(scene,0,idle_,0,palette); return; }
    dt=std::clamp(dt,0.0f,.1f);
    const float a=1-std::exp(-14*dt);
    const float speed=glm::length(glm::vec2(m.velocity.x,m.velocity.z));
    const float radians=glm::radians(yaw);
    const glm::vec2 forward{std::sin(radians),std::cos(radians)}, side{std::cos(radians),-std::sin(radians)};
    const glm::vec2 velocity{m.velocity.x,m.velocity.z};
    const float longitudinal=glm::dot(velocity,forward);
    const bool backwards=longitudinal < -0.1f;
    float direction=speed>.1f ? std::atan2(glm::dot(velocity,side),longitudinal) : 0;
    if (backwards) direction=std::remainder(direction+glm::pi<float>(),glm::two_pi<float>());
    v.direction=smooth(v.direction,direction,a);
    const glm::vec3 acc=dt>.0001f ? (m.velocity-v.previous_velocity)/dt : glm::vec3{0};
    const glm::vec2 accel{glm::dot(glm::vec2(acc.x,acc.z),forward),glm::dot(glm::vec2(acc.x,acc.z),side)};
    v.lean=glm::mix(v.lean,glm::clamp(accel*.012f,-.18f,.18f),1-std::exp(-6*dt));
    const bool just_landed=!v.motion.grounded && m.grounded && !m.mantling;
    if (just_landed) v.landing=std::clamp(std::max(m.impact,-v.previous_velocity.y)/9.0f,.15f,1.0f);
    v.landing=std::max(0.0f,v.landing-dt*1.7f);
    v.crouch=smooth(v.crouch,m.stance==Stance::Crouched?1.0f:0.0f,a);
    v.prone=smooth(v.prone,m.stance==Stance::Prone?1.0f:0.0f,1-std::exp(-9*dt));
    v.air=smooth(v.air,!m.grounded && !m.mantling?1.0f:0.0f,a);
    v.mantle_weight=smooth(v.mantle_weight,m.mantling?1.0f:0.0f,a);
    v.reach=m.interacting || m.pushing || m.pulling ? 1.0f : std::max(0.0f,v.reach-dt*2.8f);
    v.ground=glm::mix(v.ground,glm::vec2(m.left_ground,m.right_ground),a);
    const float yaw_rate=dt>.0001f ? std::remainder(yaw-v.previous_yaw,360.0f)/dt : 0;
    MoveState state=MoveState::Idle;
    if (m.mantling) state=MoveState::Mantle;
    else if (!m.grounded) state=m.velocity.y>0?MoveState::Jump:MoveState::Fall;
    else if (v.landing>.45f) state=MoveState::Land;
    else if (v.landing>.03f) state=MoveState::Recover;
    else if (m.stance==Stance::Prone) state=MoveState::Crawl;
    else if (m.stance==Stance::Crouched) state=speed>.1f?MoveState::CrouchWalk:MoveState::Crouch;
    else if (m.pulling) state=MoveState::Pull;
    else if (m.pushing) state=MoveState::Push;
    else if (v.reach>.05f) state=MoveState::Reach;
    else if (speed>.1f) {
        if (glm::dot(m.velocity,v.previous_velocity)<0) state=MoveState::Pivot;
        else if (speed-v.speed>.08f) state=MoveState::Start;
        else if (speed-v.speed<-.08f) state=MoveState::Stop;
        else state=speed>4.2f?MoveState::Sprint:speed>2.2f?MoveState::Run:MoveState::Walk;
    } else if (std::abs(yaw_rate)>10) state=MoveState::Turn;
    v.state_time=state==v.state ? v.state_time+dt : 0;
    v.state=state;
    v.clock+=dt;
    const float run_weight=weight(speed,1.9f,3.5f)*(1-v.crouch)*(1-v.prone);
    const float duration_walk=walk_>=0?scene.animations[walk_].duration:1;
    const float duration_run=run_>=0?scene.animations[run_].duration:1;
    const float stride=smooth(walk_stride_*duration_walk,run_stride_*duration_run,run_weight);
    v.phase+=dt*speed/std::max(stride,.4f)*(backwards?-1.0f:1.0f)*(1-v.air)*(1-v.mantle_weight);
    v.phase=std::remainder(v.phase,1.0f);
    std::vector<glm::vec3> t,s,bt,bs; std::vector<glm::quat> r,br;
    sample_clip(scene,walk_,v.phase*duration_walk,t,r,s);
    sample_clip(scene,run_,v.phase*duration_run,bt,br,bs);
    blend_pose(t,r,s,bt,br,bs,run_weight);
    sample_clip(scene,idle_,v.clock,bt,br,bs);
    blend_pose(t,r,s,bt,br,bs,std::max(1-weight(speed,.03f,.7f),std::max(v.air,v.mantle_weight)));
    std::vector<glm::mat4> base_globals;
    compute_globals(scene,t,r,s,base_globals);
    t[hip_].x=hip_origin_.x; t[hip_].z=hip_origin_.z;
    const auto rotate=[&](int n,glm::vec3 angles) {
        if(n>=0) r[n]=glm::normalize(glm::quat(angles)*r[n]);
    };
    const float cycle=v.phase*glm::two_pi<float>();
    const float fatigue=1-m.stamina*.01f;
    const float injury=weight(40-m.health,0,35);
    const float crouch=v.crouch;
    const float landing=v.landing*(1-v.prone);
    const float pelvis_drop=.39f*crouch+.20f*landing+.70f*v.prone;
    t[hip_].y-=pelvis_drop*units_;
    rotate(hip_,{v.prone*1.48f,v.direction*(1-v.air)*(1-v.prone),0});
    rotate(spine_,{(m.pushing?.18f:m.pulling?-.12f:0)+.30f*crouch+.4f*landing+v.lean.x+.13f*fatigue,-v.direction*.65f*(1-v.air)*(1-v.prone),-v.lean.y});
    rotate(chest_,{-.3f*v.prone,-v.direction*.35f*(1-v.air)*(1-v.prone),.1f*injury});
    const float look=glm::radians(std::clamp(std::remainder(m.view_yaw-yaw,360.0f),-70.0f,70.0f));
    rotate(chest_,{-glm::radians(m.pitch)*.25f,look*.35f,0});
    rotate(head_,{-.45f*v.prone-glm::radians(m.pitch)*.55f,look*.65f,0});
    for (int i=0;i<2;++i) {
        const float sign=i==0?1.0f:-1.0f;
        const float wave=std::sin(cycle+ (i==0?0:glm::pi<float>()));
        const float jump_fold=(.40f+weight(m.velocity.y,-2,4)*.30f)*v.air;
        rotate(thigh_[i],{-.85f*crouch-.50f*landing-jump_fold*(i==0?1.0f:.6f),0,0});
        rotate(shin_[i],{1.65f*crouch+1.0f*landing+jump_fold*1.4f,0,0});
        rotate(foot_[i],{-.8f*crouch-.5f*landing-jump_fold*.4f,0,0});
        rotate(arm_[i],{-.30f*v.air,0,sign*(.20f*v.air+.15f*fatigue)});
        if (v.prone>.001f) {
            // A separate crawl pose, blended in local quaternion space from the current pose.
            const auto pose=[&](int n,glm::vec3 angles) {
                if(n>=0) r[n]=glm::slerp(r[n],glm::quat(angles)*scene.nodes[n].rotation,v.prone);
            };
            const float moving=weight(speed,0,.4f);
            pose(thigh_[i],{-.15f-.28f*wave*moving,0,sign*.16f});
            pose(shin_[i],{.35f+.40f*std::max(0.0f,wave)*moving,0,0});
            pose(foot_[i],{.3f,0,0});
            pose(arm_[i],{-.35f,sign*.65f,sign*(-1.15f+.28f*wave*moving)});
            pose(forearm_[i],{0,sign*.7f,sign*-.5f});
        }
        const float reach=std::max(v.reach,v.mantle_weight*(1-.6f*m.mantle));
        rotate(arm_[i],{0,sign*-1.15f*reach,sign*.45f*reach});
        rotate(forearm_[i],{0,sign*-.5f*reach,0});
        if (i==0) rotate(thigh_[i],{.14f*injury*std::sin(cycle),0,.06f*injury});
    }
    // Inertialize the final pose, including interrupted transitions.
    if (v.translation.size()==t.size()) {
        blend_pose(t,r,s,v.translation,v.rotation,v.scale,std::exp(-24*dt));
    }
    v.translation=t; v.rotation=r; v.scale=s;
    std::vector<glm::mat4> globals;
    compute_globals(scene,t,r,s,globals);
    // Analytic two-bone IK in model space. Bend toward the character's front.
    if (m.grounded && !m.mantling && v.prone<.05f && v.air<.1f) {
        for(int i=0;i<2;++i) {
            const int hip=thigh_[i],knee=shin_[i],ankle=foot_[i];
            if(hip<0 || knee<0 || ankle<0) continue;
            const glm::vec3 h=globals[hip][3], k=globals[knee][3], f=globals[ankle][3];
            glm::vec3 target=f;
            target.y=base_globals[ankle][3].y+(i==0?v.ground.x:v.ground.y)/scale_;
            const float upper=glm::length(k-h), lower=glm::length(f-k);
            const float length=std::clamp(glm::length(target-h),.01f,upper+lower-.001f);
            const glm::vec3 dir=glm::normalize(target-h);
            glm::vec3 bend=glm::vec3{0,0,1}-dir*glm::dot(dir,glm::vec3{0,0,1});
            if(glm::length(bend)<.001f) bend={1,0,0};
            bend=glm::normalize(bend);
            const float along=(upper*upper+length*length-lower*lower)/(2*length);
            const glm::vec3 desired_knee=h+dir*along+bend*std::sqrt(std::max(0.0f,upper*upper-along*along));
            const auto aim=[&](int bone,glm::vec3 from,glm::vec3 to) {
                if(glm::length(from)<.001f || glm::length(to)<.001f) return;
                const int parent=scene.nodes[bone].parent;
                const glm::quat parent_q=parent<0?glm::quat(1,0,0,0):glm::quat_cast(glm::mat3(globals[parent])/glm::length(glm::vec3(globals[parent][0])));
                const glm::quat delta=glm::rotation(glm::normalize(from),glm::normalize(to));
                r[bone]=glm::normalize(glm::inverse(parent_q)*delta*parent_q*r[bone]);
            };
            aim(hip,k-h,desired_knee-h);
            compute_globals(scene,t,r,s,globals);
            aim(knee,glm::vec3(globals[ankle][3])-glm::vec3(globals[knee][3]),target-glm::vec3(globals[knee][3]));
            compute_globals(scene,t,r,s,globals);
            const auto orientation=[](const glm::mat4& matrix) {
                return glm::normalize(glm::quat_cast(glm::mat3(matrix)/glm::length(glm::vec3(matrix[0]))));
            };
            r[ankle]=glm::inverse(orientation(globals[knee]))*orientation(base_globals[ankle]);
            compute_globals(scene,t,r,s,globals);
        }
    }
    compute_palette(scene,0,globals,palette);
    v.previous_velocity=m.velocity; v.previous_yaw=yaw; v.speed=speed; v.motion=m;
}

} // namespace forge::anim
