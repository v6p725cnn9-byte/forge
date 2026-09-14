#include "engine/anim/locomotion/locomotion.hpp"
#include "engine/physics/world/world.hpp"
#include "engine/game/training/course.hpp"
#include <cstdlib>
#include <iostream>
#include <cmath>

namespace {
void check(bool ok,const char* reason) { if(!ok) { std::cerr<<reason<<'\n'; std::exit(1); } }
constexpr float dt=1.0f/60;
int joint(const forge::assets::Scene& scene,const char* name) {
    for (std::size_t i=0;i<scene.skins[0].joints.size();++i)
        if (scene.nodes[scene.skins[0].joints[i]].name==std::string("mixamorig:")+name) return static_cast<int>(i);
    return -1;
}
glm::vec3 position(const forge::assets::Scene& scene,const forge::anim::Palette& palette,int joint) {
    return glm::vec3(palette.joints[joint]*glm::inverse(scene.skins[0].inverse_bind[joint])*glm::vec4{0,0,0,1});
}
}
int main() {
    using namespace forge;
    assets::Scene scene; std::string error;
    const char* root=std::getenv("FORGE_TEST_ROOT"); check(root,"root");
    check(assets::load_gltf(std::string(root)+"/assets/models/Als/Mannequin.glb",scene,error),error.c_str());
    anim::Locomotion animation; animation.bind(scene,1);
    anim::LocomotionState state; anim::Palette pose;
    Motion motion;
    auto animate=[&](int frames) { for(int i=0;i<frames;++i) animation.evaluate(scene,state,motion,0,dt,pose); };
    animate(90);
    const int hip=joint(scene,"Hips"),foot=joint(scene,"LeftFoot"),head=joint(scene,"Head");
    check(hip>=0 && foot>=0 && head>=0,"rig mapped");
    const float idle_hip=position(scene,pose,hip).y;
    const float idle_foot=position(scene,pose,foot).y;
    motion.stance=Stance::Crouched; animate(90);
    check(idle_hip-position(scene,pose,hip).y>.3f,"crouch lowers pelvis");
    check(std::abs(position(scene,pose,foot).y-idle_foot)<.10f,"crouch retains foot contact");
    motion.left_ground=.25f; animate(90);
    check(position(scene,pose,foot).y-idle_foot>.15f,"IK raises foot onto a step");
    motion.left_ground=0; motion.stance=Stance::Prone; animate(90);
    check(position(scene,pose,head).y<.8f,"crawl head fits low tunnel");
    check(state.state==anim::MoveState::Crawl,"prone selects crawl");
    motion.stance=Stance::Standing; motion.grounded=false; motion.velocity={0,4.2f,2}; animate(20);
    check(state.state==anim::MoveState::Jump,"rising selects jump");
    motion.velocity.y=-6; animate(20); check(state.state==anim::MoveState::Fall,"descent selects fall");
    motion.grounded=true; motion.velocity={0,0,0}; motion.impact=6; animate(1);
    check(state.state==anim::MoveState::Land,"impact selects land");
    animate(120); check(state.state==anim::MoveState::Idle,"landing recovers to idle");
    motion.velocity={0,0,-3.75f}; animate(60);
    check(state.phase!=0,"backpedal advances phase");
    const auto before=position(scene,pose,head);
    motion.stance=Stance::Prone; animate(1);
    check(glm::length(position(scene,pose,head)-before)<.20f,"interrupted transition has no pose snap");
    for(int i=0;i<600;++i) {
        motion.velocity={std::sin(i*.1f)*6.5f,0,std::cos(i*.1f)*6.5f};
        motion.stance=static_cast<Stance>((i/37)%3); motion.grounded=i%43<30;
        motion.mantling=i%61<12; motion.pitch=std::sin(i*.07f)*85;
        animate(1);
        for(int j=0;j<pose.count;++j) for(int c=0;c<4;++c) for(int r=0;r<4;++r)
            check(std::isfinite(pose.joints[j][c][r]),"transitions produce finite palette");
    }
    phys::World world; check(world.init(),"world init");
    world.add_box({0,-.5f,0},{60,.5f,60});
    for(const auto& box:game::locomotion_course()) world.add_box(box.center,box.half,box.pitch);
    const int body=world.spawn_character({2,.9f,-5},.3f,.6f);
    auto tick=[&](int frames) { for(int i=0;i<frames;++i) world.tick(dt); };
    tick(30);
    check(world.set_character_stance(body,Stance::Prone),"enter prone"); tick(10);
    check(std::abs(world.character_position(body).y-.3f)<.06f,"prone capsule stays on floor");
    world.set_character_facing(body,180);
    world.set_character_input(body,{0,0,-1},false,6.5f); tick(400);
    check(world.character_position(body).z<-8.2f,"crawl enters low tunnel");
    world.set_character_input(body,{0,0,0},false); tick(60);
    check(!world.set_character_stance(body,Stance::Standing),"ceiling prevents standing");
    check(!world.set_character_stance(body,Stance::Crouched),"ceiling prevents crouching");
    check(world.character_motion(body).stance==Stance::Prone,"failed expansion retains prone");
    world.set_character_input(body,{0,0,-1},false,6.5f); tick(600);
    check(world.character_position(body).z<-14,"crawl exits tunnel");
    world.set_character_input(body,{0,0,0},false); tick(60);
    // Move back into open space between the two tunnels before standing.
    world.warp_character(body,{6,.3f,-10}); tick(30);
    check(world.set_character_stance(body,Stance::Standing),"stand in clear space");
    tick(30); check(std::abs(world.character_position(body).y-.9f)<.06f,"standing restores capsule height");
    const int stairs=world.spawn_character({-12,.9f,-4.5f},.3f,.6f); tick(30);
    world.set_character_input(stairs,{0,0,-1},false,1.75f); tick(230);
    check(world.character_position(stairs).y>1.8f,"Jolt walks stairs");
    const int slope=world.spawn_character({-16,.9f,-14.5f},.3f,.6f); tick(30);
    world.set_character_input(slope,{0,0,-1},false,1.75f); tick(180);
    check(world.character_position(slope).y>1.5f,"Jolt climbs 30 degree ramp");
    const int crate=world.add_crate({6,.55f,-5});
    world.warp_character(body,{6,.9f,-3.8f}); world.set_character_facing(body,180); tick(30);
    const float z=world.box_position(crate).z;
    world.set_character_input(body,{0,0,-1},false,1.75f); tick(100);
    check(world.box_position(crate).z<z-.3f,"character pushes dynamic crate");
    std::cout<<"Locomotion pose, stance, terrain and crate checks passed\n";
}
