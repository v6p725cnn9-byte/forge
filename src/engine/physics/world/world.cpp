#include "engine/physics/world/world.hpp"
#include "engine/physics/locomotion/als.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/RegisterTypes.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace forge::phys {
namespace {

constexpr JPH::ObjectLayer layer_static = 0;
constexpr JPH::ObjectLayer layer_moving = 1;
constexpr JPH::BroadPhaseLayer bp_static{0};
constexpr JPH::BroadPhaseLayer bp_moving{1};

class Broadphase : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        return layer == layer_static ? bp_static : bp_moving;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "layer"; }
#endif
};

class ObjectVsBroadphase : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override
    {
        if (layer == layer_static) return bp == bp_moving;
        return true;
    }
};

class ObjectPair : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
    {
        if (a == layer_static) return b == layer_moving;
        if (b == layer_static) return a == layer_moving;
        return true;
    }
};

JPH::Vec3 to_jolt(glm::vec3 v) { return {v.x, v.y, v.z}; }
glm::vec3 to_glm(JPH::RVec3 v)
{
    return {static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ())};
}
glm::mat4 to_glm(const JPH::RMat44& m)
{
    glm::mat4 out(1.0f);
    const auto x = m.GetAxisX();
    const auto y = m.GetAxisY();
    const auto z = m.GetAxisZ();
    const auto t = m.GetTranslation();
    out[0] = {x.GetX(), x.GetY(), x.GetZ(), 0.0f};
    out[1] = {y.GetX(), y.GetY(), y.GetZ(), 0.0f};
    out[2] = {z.GetX(), z.GetY(), z.GetZ(), 0.0f};
    out[3] = {static_cast<float>(t.GetX()), static_cast<float>(t.GetY()), static_cast<float>(t.GetZ()), 1.0f};
    return out;
}

struct RayHit {
    bool hit = false;
    float fraction = 1.0f;
    glm::vec3 point{0};
    glm::vec3 normal{0, 1, 0};
};

RayHit cast_ray(JPH::PhysicsSystem& system, JPH::RVec3 start, JPH::Vec3 dir)
{
    RayHit out;
    JPH::RayCastResult result;
    if (!system.GetNarrowPhaseQuery().CastRay(JPH::RRayCast{start, dir}, result,
                                              system.GetDefaultBroadPhaseLayerFilter(layer_moving),
                                              system.GetDefaultLayerFilter(layer_moving)))
        return out;
    out.hit = true;
    out.fraction = result.mFraction;
    out.point = {static_cast<float>(start.GetX() + dir.GetX() * result.mFraction),
                 static_cast<float>(start.GetY() + dir.GetY() * result.mFraction),
                 static_cast<float>(start.GetZ() + dir.GetZ() * result.mFraction)};
    JPH::BodyLockRead lock(system.GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        const auto n = lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2,
                                                                 {out.point.x, out.point.y, out.point.z});
        out.normal = {n.GetX(), n.GetY(), n.GetZ()};
    }
    return out;
}

bool walkable(glm::vec3 normal)
{
    return normal.y >= std::cos(glm::radians(als::kMaxSlopeDegrees));
}

RayHit cast_capsule(JPH::PhysicsSystem& system, glm::vec3 origin, glm::vec3 delta, float cylinder_half, float radius)
{
    RayHit out;
    JPH::RefConst<JPH::Shape> shape = new JPH::CapsuleShape(cylinder_half, radius);
    const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(
        shape, JPH::Vec3::sReplicate(1.0f), JPH::RMat44::sTranslation({origin.x, origin.y, origin.z}),
        {delta.x, delta.y, delta.z});
    JPH::ShapeCastSettings settings;
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    system.GetNarrowPhaseQuery().CastShape(cast, settings, {origin.x, origin.y, origin.z}, collector,
                                           system.GetDefaultBroadPhaseLayerFilter(layer_moving),
                                           system.GetDefaultLayerFilter(layer_moving));
    if (!collector.HadHit()) return out;
    out.hit = true;
    out.fraction = collector.mHit.mFraction;
    const auto& p = collector.mHit.mContactPointOn2;
    out.point = {origin.x + p.GetX(), origin.y + p.GetY(), origin.z + p.GetZ()};
    JPH::BodyLockRead lock(system.GetBodyLockInterface(), collector.mHit.mBodyID2);
    if (lock.Succeeded()) {
        const auto n = lock.GetBody().GetWorldSpaceSurfaceNormal(collector.mHit.mSubShapeID2,
                                                                 {out.point.x, out.point.y, out.point.z});
        out.normal = {n.GetX(), n.GetY(), n.GetZ()};
    }
    return out;
}

} // namespace

struct World::Impl {
    struct Character {
        JPH::Ref<JPH::CharacterVirtual> body;
        JPH::RefConst<JPH::Shape> shape;
        glm::vec3 walk{0};
        als::State move{};
        glm::vec3 mantle_from{0};
        glm::vec3 mantle_to{0};
        float mantle_yaw_from = 0.0f;
        float mantle_yaw_to = 0.0f;
        float mantle_time = 0.0f;
        float mantle_duration = 0.0f;
        float speed = als::Settings{}.run_forward;
        float facing_yaw = 0.0f;
        float rest_height = als::kCapsuleHalfHeight;
        float radius = als::kCapsuleRadius;
        float mantle_cooldown = 0.0f;
        Stance stance = Stance::Standing;
        float standing_height = 0.9f;
        float impact = 0;
        bool jump = false;
        bool enabled = true;
        bool mantling = false;
    };

    Broadphase broadphase;
    ObjectVsBroadphase object_vs_bp;
    ObjectPair object_pair;
    std::unique_ptr<JPH::TempAllocatorImpl> allocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobs;
    JPH::PhysicsSystem system;
    JPH::BodyInterface* bodies = nullptr;
    std::vector<Character> characters;
    std::vector<JPH::BodyID> boxes;
    JPH::Body* car = nullptr;
    JPH::Ref<JPH::VehicleConstraint> vehicle;
    JPH::RefConst<JPH::VehicleCollisionTester> tester;
    float vehicle_forward = 0.0f;
    float vehicle_steer = 0.0f;
    float vehicle_brake = 0.0f;
    bool registered = false;
};

World::World() : impl_(std::make_unique<Impl>()) {}
World::~World()
{
    if (!impl_ || !impl_->registered) return;
    if (impl_->vehicle) {
        impl_->system.RemoveStepListener(impl_->vehicle.GetPtr());
        impl_->system.RemoveConstraint(impl_->vehicle.GetPtr());
    }
    impl_->vehicle = nullptr;
    impl_->tester = nullptr;
    if (impl_->car && impl_->bodies) {
        impl_->bodies->RemoveBody(impl_->car->GetID());
        impl_->bodies->DestroyBody(impl_->car->GetID());
    }
    impl_->car = nullptr;
    impl_->characters.clear();
    impl_.reset();
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

bool World::init()
{
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    impl_->registered = true;
    impl_->allocator = std::make_unique<JPH::TempAllocatorImpl>(8 * 1024 * 1024);
    impl_->jobs = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1);
    impl_->system.Init(1024, 0, 2048, 1024, impl_->broadphase, impl_->object_vs_bp, impl_->object_pair);
    impl_->system.SetGravity({0, -als::Settings{}.gravity, 0});
    impl_->bodies = &impl_->system.GetBodyInterface();
    return true;
}

int World::add_box(glm::vec3 center, glm::vec3 half_extents, float pitch_degrees)
{
    auto shape = new JPH::BoxShape(to_jolt(half_extents));
    JPH::BodyCreationSettings settings(shape, to_jolt(center), JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::DegreesToRadians(pitch_degrees)), JPH::EMotionType::Static,
                                       layer_static);
    const auto id = impl_->bodies->CreateAndAddBody(settings, JPH::EActivation::DontActivate);
    if (id.IsInvalid()) return -1;
    impl_->boxes.push_back(id);
    return static_cast<int>(impl_->boxes.size() - 1);
}

int World::add_crate(glm::vec3 center)
{
    JPH::BodyCreationSettings settings(new JPH::BoxShape({.55f,.55f,.55f}), to_jolt(center),
        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, layer_moving);
    settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = 45.0f;
    settings.mFriction = .6f;
    const auto id = impl_->bodies->CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (id.IsInvalid()) return -1;
    impl_->boxes.push_back(id);
    return static_cast<int>(impl_->boxes.size()-1);
}

glm::vec3 World::box_position(int box) const
{
    if (box < 0 || box >= static_cast<int>(impl_->boxes.size()) || impl_->boxes[box].IsInvalid()) return {};
    return to_glm(impl_->bodies->GetPosition(impl_->boxes[box]));
}

bool World::pull_box(int box, int character)
{
    if (box < 0 || box >= static_cast<int>(impl_->boxes.size()) || impl_->boxes[box].IsInvalid()
        || character < 0 || character >= static_cast<int>(impl_->characters.size())) return false;
    const auto& c = impl_->characters[character];
    if (!c.body || c.stance != Stance::Standing || c.mantling || !character_supported(character)) return false;
    const glm::vec3 position = to_glm(c.body->GetPosition());
    const float yaw = glm::radians(c.facing_yaw);
    const glm::vec3 forward{std::sin(yaw),0,std::cos(yaw)};
    glm::vec3 delta = box_position(box)-position;
    if (std::abs(delta.y) > 1.0f) return false;
    delta.y = 0;
    if (glm::length(delta)>2.0f || glm::dot(delta,forward)<.25f) return false;
    // A spring force lets Jolt resolve walls and the player's capsule during pulling.
    const glm::vec3 target = position+forward*1.1f;
    glm::vec3 force=(target-box_position(box))*450.0f-to_glm(impl_->bodies->GetLinearVelocity(impl_->boxes[box]))*65.0f;
    force.y=0;
    if (glm::length(force)>600) force=glm::normalize(force)*600.0f;
    impl_->bodies->AddForce(impl_->boxes[box],to_jolt(force));
    return true;
}

void World::remove_box(int box)
{
    if (box < 0 || box >= static_cast<int>(impl_->boxes.size())) return;
    const auto id = impl_->boxes[static_cast<std::size_t>(box)];
    if (id.IsInvalid()) return;
    impl_->bodies->RemoveBody(id);
    impl_->bodies->DestroyBody(id);
    impl_->boxes[static_cast<std::size_t>(box)] = {};
}

int World::spawn_character(glm::vec3 position, float radius, float half_height)
{
    Impl::Character character;
    character.shape = new JPH::CapsuleShape(half_height, radius);
    JPH::CharacterVirtualSettings settings;
    settings.mShape = character.shape;
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);
    settings.mMass = als::kMass;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(als::kMaxSlopeDegrees);
    character.body = new JPH::CharacterVirtual(&settings, to_jolt(position), JPH::Quat::sIdentity(), 0, &impl_->system);
    character.radius = radius;
    character.rest_height = half_height + radius;
    character.standing_height = character.rest_height;
    impl_->characters.push_back(std::move(character));
    return static_cast<int>(impl_->characters.size() - 1);
}

bool World::set_character_stance(int character, Stance stance)
{
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return false;
    auto& c = impl_->characters[character];
    if (!c.body || c.mantling) return false;
    if (c.stance == stance) return true;
    if (c.body->GetGroundState() != JPH::CharacterVirtual::EGroundState::OnGround) return false;
    const float half = stance == Stance::Standing ? c.standing_height : stance_height(stance) * 0.5f;
    JPH::RefConst<JPH::Shape> shape;
    if (stance == Stance::Prone) {
        JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(0.60f, c.radius);
        shape = JPH::RotatedTranslatedShapeSettings(JPH::Vec3::sZero(),
            JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::JPH_PI * .5f), capsule).Create().Get();
    }
    else shape = new JPH::CapsuleShape(half - c.radius, c.radius);
    const auto previous = c.body->GetPosition();
    c.body->SetPosition(previous + JPH::Vec3(0, half - c.rest_height, 0));
    if (!c.body->SetShape(shape, 0.02f, impl_->system.GetDefaultBroadPhaseLayerFilter(layer_moving),
                          impl_->system.GetDefaultLayerFilter(layer_moving), {}, {}, *impl_->allocator)) {
        c.body->SetPosition(previous);
        return false;
    }
    c.shape = shape;
    c.rest_height = half;
    c.stance = stance;
    return true;
}

Motion World::character_motion(int character) const
{
    Motion result;
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return result;
    const auto& c = impl_->characters[character];
    if (!c.body) return result;
    result.velocity = to_glm(c.body->GetLinearVelocity());
    result.stance = c.stance;
    result.grounded = c.body->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
    result.mantling = c.mantling;
    result.mantle = c.mantling ? std::clamp(c.mantle_time / c.mantle_duration, 0.0f, 1.0f) : 0;
    result.impact = c.impact;
    result.view_yaw = c.facing_yaw;
    const glm::vec3 center = to_glm(c.body->GetPosition());
    const float feet = center.y - c.rest_height;
    const float yaw = glm::radians(c.move.actor_yaw);
    for (int i = 0; i < 2; ++i) {
        const float side = i == 0 ? -0.18f : 0.18f;
        const auto hit = cast_ray(impl_->system,
            {center.x + std::cos(yaw) * side, feet + 0.5f, center.z - std::sin(yaw) * side}, {0,-1,0});
        const float offset = result.grounded && hit.hit ? std::clamp(hit.point.y - feet, -0.4f, 0.4f) : 0;
        (i == 0 ? result.left_ground : result.right_ground) = offset;
    }
    return result;
}

bool World::spawn_vehicle(glm::vec3 position, float yaw_degrees)
{
    if (impl_->vehicle) return true;
    constexpr float half_length = 1.8f;
    constexpr float half_width = 0.85f;
    constexpr float half_height = 0.22f;
    constexpr float wheel_radius = 0.32f;
    constexpr float wheel_width = 0.18f;
    auto car_shape =
        JPH::OffsetCenterOfMassShapeSettings(JPH::Vec3(0, -half_height, 0), new JPH::BoxShape({half_width, half_height, half_length}))
            .Create()
            .Get();
    JPH::BodyCreationSettings body_settings(car_shape, to_jolt(position),
                                            JPH::Quat::sRotation(JPH::Vec3::sAxisY(), JPH::DegreesToRadians(yaw_degrees)),
                                            JPH::EMotionType::Dynamic, layer_moving);
    body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    body_settings.mMassPropertiesOverride.mMass = 1400.0f;
    impl_->car = impl_->bodies->CreateBody(body_settings);
    if (!impl_->car) return false;
    impl_->bodies->AddBody(impl_->car->GetID(), JPH::EActivation::Activate);

    JPH::VehicleConstraintSettings vehicle;
    vehicle.mMaxPitchRollAngle = JPH::DegreesToRadians(55.0f);

    auto* fl = new JPH::WheelSettingsWV;
    fl->mPosition = JPH::Vec3(half_width, -0.18f, 1.35f);
    fl->mMaxSteerAngle = JPH::DegreesToRadians(35.0f);
    fl->mMaxHandBrakeTorque = 0.0f;
    auto* fr = new JPH::WheelSettingsWV;
    fr->mPosition = JPH::Vec3(-half_width, -0.18f, 1.35f);
    fr->mMaxSteerAngle = JPH::DegreesToRadians(35.0f);
    fr->mMaxHandBrakeTorque = 0.0f;
    auto* bl = new JPH::WheelSettingsWV;
    bl->mPosition = JPH::Vec3(half_width, -0.18f, -1.35f);
    bl->mMaxSteerAngle = 0.0f;
    auto* br = new JPH::WheelSettingsWV;
    br->mPosition = JPH::Vec3(-half_width, -0.18f, -1.35f);
    br->mMaxSteerAngle = 0.0f;
    vehicle.mWheels = {fl, fr, bl, br};
    for (JPH::WheelSettings* wheel : vehicle.mWheels) {
        wheel->mRadius = wheel_radius;
        wheel->mWidth = wheel_width;
        wheel->mSuspensionMinLength = 0.25f;
        wheel->mSuspensionMaxLength = 0.45f;
    }

    auto* controller = new JPH::WheeledVehicleControllerSettings;
    controller->mDifferentials.resize(1);
    controller->mDifferentials[0].mLeftWheel = 0;
    controller->mDifferentials[0].mRightWheel = 1;
    vehicle.mController = controller;
    vehicle.mAntiRollBars.resize(2);
    vehicle.mAntiRollBars[0].mLeftWheel = 0;
    vehicle.mAntiRollBars[0].mRightWheel = 1;
    vehicle.mAntiRollBars[1].mLeftWheel = 2;
    vehicle.mAntiRollBars[1].mRightWheel = 3;

    impl_->vehicle = new JPH::VehicleConstraint(*impl_->car, vehicle);
    static_cast<JPH::WheeledVehicleController*>(impl_->vehicle->GetController())
        ->SetTireMaxImpulseCallback(
            [](JPH::uint, float& longitudinal, float& lateral, float suspension, float long_friction, float lat_friction,
               float, float, float) {
                longitudinal = 10.0f * long_friction * suspension;
                lateral = lat_friction * suspension;
            });
    impl_->tester = new JPH::VehicleCollisionTesterRay(layer_moving);
    impl_->vehicle->SetVehicleCollisionTester(impl_->tester);
    impl_->system.AddConstraint(impl_->vehicle.GetPtr());
    impl_->system.AddStepListener(impl_->vehicle.GetPtr());
    return true;
}

void World::remove_character(int character)
{
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return;
    impl_->characters[static_cast<std::size_t>(character)] = {};
}

void World::set_character_enabled(bool enabled) { set_character_enabled(0, enabled); }

void World::set_character_enabled(int character, bool enabled)
{
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return;
    auto& state = impl_->characters[static_cast<std::size_t>(character)];
    if (state.body) state.enabled = enabled;
}

void World::warp_character(glm::vec3 position)
{
    warp_character(0, position);
}

void World::warp_character(int character, glm::vec3 position)
{
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return;
    auto& state = impl_->characters[static_cast<std::size_t>(character)];
    if (!state.body) return;
    state.body->SetPosition(to_jolt(position));
    state.body->SetLinearVelocity(JPH::Vec3::sZero());
    state.move.planar = {};
    state.move.vertical = 0.0f;
    state.mantling = false;
    state.mantle_time = 0.0f;
}

void World::set_character_input(int character, glm::vec3 walk_xz, bool jump, float speed)
{
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return;
    auto& state = impl_->characters[static_cast<std::size_t>(character)];
    if (!state.body) return;
    state.walk = walk_xz;
    state.jump = jump;
    state.speed = std::max(0.0f, speed);
}

bool World::try_mantle(int character, glm::vec3 wish_dir_xz)
{
    if (!impl_->registered || character < 0 || character >= static_cast<int>(impl_->characters.size())) return false;
    auto& state = impl_->characters[static_cast<std::size_t>(character)];
    if (!state.body || !state.enabled || state.mantling || state.mantle_cooldown > 0.0f || state.stance != Stance::Standing) return false;
    const bool grounded = state.body->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
    const float max_rise = grounded ? als::kMantleMaxRise : als::kMantleMaxRiseAir;
    const float reach = grounded ? als::kMantleReach : als::kMantleReachAir;
    const glm::vec2 wish{wish_dir_xz.x, wish_dir_xz.z};
    const bool has_input = glm::length(wish) >= 0.3f;
    const bool has_velocity = glm::length(state.move.planar) >= 0.01f;
    const float actor = state.move.actor_yaw;
    float forward_angle = actor;
    if (has_velocity) {
        if (has_input) {
            const float input_yaw = glm::degrees(std::atan2(wish.x, wish.y));
            float delta = als::unwind_degrees(input_yaw - state.move.velocity_yaw);
            delta = std::clamp(delta, -als::kMantleMaxReachAngle, als::kMantleMaxReachAngle);
            forward_angle = state.move.velocity_yaw + delta;
        } else {
            forward_angle = state.move.velocity_yaw;
        }
    } else if (has_input) {
        forward_angle = glm::degrees(std::atan2(wish.x, wish.y));
    }
    const float delta_actor = als::unwind_degrees(forward_angle - actor);
    if (std::abs(delta_actor) > als::kMantleTraceAngle) return false;
    const float dir_yaw = glm::radians(actor + std::clamp(delta_actor, -als::kMantleMaxReachAngle, als::kMantleMaxReachAngle));
    const glm::vec2 dir{std::sin(dir_yaw), std::cos(dir_yaw)};
    const JPH::RVec3 center = state.body->GetPosition();
    const float feet = static_cast<float>(center.GetY()) - state.rest_height;
    const float trace_radius = std::max(0.05f, state.radius - 0.01f);
    const float ue_half = 0.5f * (max_rise - als::kMantleMinRise);
    const float cyl_half = std::max(0.02f, ue_half - trace_radius);
    const glm::vec3 forward_start{static_cast<float>(center.GetX()) - dir.x * state.radius,
                                  feet + 0.5f * (als::kMantleMinRise + max_rise) - 0.024f,
                                  static_cast<float>(center.GetZ()) - dir.y * state.radius};
    const glm::vec3 forward_delta{dir.x * (state.radius + reach + 0.01f), 0.0f,
                                  dir.y * (state.radius + reach + 0.01f)};
    const RayHit wall = cast_capsule(impl_->system, forward_start, forward_delta, cyl_half, trace_radius);
    if (!wall.hit || walkable(wall.normal)) return false;
    const glm::vec2 target_dir{-wall.normal.x, -wall.normal.z};
    const float tlen = glm::length(target_dir);
    const glm::vec2 into = tlen > 1e-4f ? target_dir / tlen : dir;
    const glm::vec2 land{wall.point.x + into.x * (state.radius + 0.08f),
                         wall.point.z + into.y * (state.radius + 0.08f)};
    const float drop_len = max_rise + 0.05f;
    const JPH::RVec3 drop{land.x, feet + max_rise + 0.05f, land.y};
    const RayHit top = cast_ray(impl_->system, drop, JPH::Vec3(0.0f, -drop_len, 0.0f));
    if (!top.hit || top.normal.y < als::kMantleSlopeCos || !walkable(top.normal)) return false;
    const float rise = top.point.y - feet;
    if (rise < als::kMantleMinRise || rise > max_rise) return false;
    const JPH::RVec3 ceiling{land.x, top.point.y + 0.02f, land.y};
    const RayHit roof = cast_ray(impl_->system, ceiling, JPH::Vec3(0.0f, 2.0f * state.rest_height, 0.0f));
    if (roof.hit && roof.fraction * 2.0f * state.rest_height < 2.0f * state.rest_height - 0.05f) return false;
    const glm::vec3 from = to_glm(center);
    const glm::vec3 to_pos{land.x, top.point.y + state.rest_height, land.y};
    state.mantling = true;
    state.mantle_from = from;
    state.mantle_to = to_pos;
    state.mantle_yaw_from = state.move.actor_yaw;
    state.mantle_yaw_to = glm::degrees(std::atan2(into.x, into.y));
    state.mantle_time = 0.0f;
    const bool high = rise > als::kMantleHigh;
    state.mantle_duration = !grounded ? als::kMantleAirSeconds
        : (high ? als::kMantleHighSeconds : als::kMantleLowSeconds);
    state.mantle_cooldown = als::kMantleCooldown;
    state.move.planar = {};
    state.move.vertical = 0.0f;
    return true;
}

void World::set_character_facing(int character, float yaw_degrees)
{
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return;
    auto& state = impl_->characters[static_cast<std::size_t>(character)];
    if (!state.body) return;
    state.facing_yaw = yaw_degrees;
    if (!state.move.yaw_initialized) {
        state.move.actor_yaw = yaw_degrees;
        state.move.target_yaw = yaw_degrees;
        state.move.smooth_target_yaw = yaw_degrees;
        state.move.previous_view_yaw = yaw_degrees;
        state.move.view_yaw = yaw_degrees;
        state.move.yaw_initialized = true;
    }
}

void World::set_vehicle_input(float forward, float steer, float brake)
{
    impl_->vehicle_forward = forward;
    impl_->vehicle_steer = steer;
    impl_->vehicle_brake = brake;
}

void World::tick(float dt, glm::vec3 walk_xz, bool jump)
{
    set_character_input(0, walk_xz, jump);
    tick(dt);
}

void World::tick(float dt)
{
    if (impl_->vehicle && impl_->car) {
        auto* controller = static_cast<JPH::WheeledVehicleController*>(impl_->vehicle->GetController());
        controller->SetDriverInput(impl_->vehicle_forward, impl_->vehicle_steer, impl_->vehicle_brake, 0.0f);
        if (std::abs(impl_->vehicle_forward) + std::abs(impl_->vehicle_steer) + impl_->vehicle_brake > 0.01f)
            impl_->bodies->ActivateBody(impl_->car->GetID());
    }
    for (auto& character : impl_->characters) {
        if (!character.body || !character.enabled) continue;
        if (character.mantle_cooldown > 0.0f) character.mantle_cooldown = std::max(0.0f, character.mantle_cooldown - dt);
        if (character.mantling) {
            character.mantle_time += dt;
            const float span = std::max(character.mantle_duration, 1e-4f);
            float alpha = std::clamp(character.mantle_time / span, 0.0f, 1.0f);
            alpha = alpha * alpha * (3.0f - 2.0f * alpha);
            glm::vec3 pos = glm::mix(character.mantle_from, character.mantle_to, alpha);
            // Raise the capsule above the lip before translating onto the ledge.
            const float lift = std::clamp(character.mantle_time / span * 2.0f, 0.0f, 1.0f);
            pos.y = glm::mix(character.mantle_from.y, character.mantle_to.y, lift * lift * (3 - 2 * lift));
            character.body->SetPosition(to_jolt(pos));
            character.body->SetLinearVelocity({0, 0, 0});
            character.move.planar = {};
            character.move.vertical = 0.0f;
            character.move.actor_yaw = als::unwind_degrees(
                character.mantle_yaw_from
                + als::remap_ccw(als::unwind_degrees(character.mantle_yaw_to - character.mantle_yaw_from)) * alpha);
            character.move.target_yaw = character.move.actor_yaw;
            character.move.smooth_target_yaw = character.move.actor_yaw;
            if (character.mantle_time >= span) character.mantling = false;
            character.jump = false;
            continue;
        }
        // Only walkable ground zeroes gravity: a wall contact reports
        // OnSteepGround, and treating it as grounded freezes jumps mid-air.
        const bool grounded = character.body->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
        const auto linear = character.body->GetLinearVelocity();
        character.move.planar = {linear.GetX(), linear.GetZ()};
        character.move.vertical = linear.GetY();
        character.move = als::tick(character.move, character.walk, character.facing_yaw, character.jump && character.stance == Stance::Standing, grounded,
                                   character.stance == Stance::Standing ? character.speed : std::min(character.speed, stance_speed(character.stance)), dt);
        character.body->SetRotation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(), JPH::DegreesToRadians(character.move.actor_yaw)));
        character.body->SetLinearVelocity({character.move.planar.x, character.move.vertical, character.move.planar.y});
        JPH::CharacterVirtual::ExtendedUpdateSettings update;
        update.mWalkStairsStepUp = JPH::Vec3(0, character.stance == Stance::Prone ? 0.10f : als::kStepUp, 0);
        if (character.move.vertical > 0.1f) update.mStickToFloorStepDown = JPH::Vec3::sZero();
        const float falling_speed = character.move.vertical;
        character.body->ExtendedUpdate(dt, impl_->system.GetGravity(), update,
                                       impl_->system.GetDefaultBroadPhaseLayerFilter(layer_moving),
                                       impl_->system.GetDefaultLayerFilter(layer_moving), {}, {}, *impl_->allocator);
        character.impact = std::max(0.0f, character.impact - dt * 16.0f);
        if (!grounded && character.body->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround)
            character.impact = std::max(character.impact, -falling_speed);
        const auto after = character.body->GetLinearVelocity();
        character.move.planar = {after.GetX(), after.GetZ()};
        character.move.vertical = after.GetY();
        character.jump = false;
    }
    impl_->system.Update(dt, 1, impl_->allocator.get(), impl_->jobs.get());
}

glm::vec3 World::character_position() const
{
    return character_position(0);
}

glm::vec3 World::character_position(int character) const
{
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return {};
    const auto& state = impl_->characters[static_cast<std::size_t>(character)];
    return state.body ? to_glm(state.body->GetPosition()) : glm::vec3{};
}

float World::character_yaw() const { return character_yaw(0); }

float World::character_yaw(int character) const
{
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return 0.0f;
    return impl_->characters[static_cast<std::size_t>(character)].move.actor_yaw;
}

bool World::character_supported() const { return character_supported(0); }

bool World::character_supported(int character) const
{
    if (character < 0 || character >= static_cast<int>(impl_->characters.size())) return false;
    const auto& state = impl_->characters[static_cast<std::size_t>(character)];
    if (!state.body) return false;
    return state.body->GetGroundState() != JPH::CharacterVirtual::EGroundState::InAir;
}

bool World::has_vehicle() const { return impl_->vehicle != nullptr; }

glm::mat4 World::vehicle_transform() const
{
    if (!impl_->car) return glm::mat4(1.0f);
    return to_glm(impl_->car->GetWorldTransform());
}

glm::mat4 World::wheel_transform(int index) const
{
    if (!impl_->vehicle || index < 0 || index >= 4) return glm::mat4(1.0f);
    const JPH::Vec3 right = (index == 0 || index == 2) ? -JPH::Vec3::sAxisX() : JPH::Vec3::sAxisX();
    return to_glm(impl_->vehicle->GetWheelWorldTransform(static_cast<JPH::uint>(index), right, JPH::Vec3::sAxisY()));
}

glm::vec3 World::vehicle_position() const
{
    if (!impl_->car) return {};
    return to_glm(impl_->car->GetPosition());
}

float World::vehicle_speed() const
{
    if (!impl_->car) return 0.0f;
    return impl_->car->GetLinearVelocity().Length();
}

float World::vehicle_yaw() const
{
    if (!impl_->car) return 0.0f;
    const auto forward = impl_->car->GetRotation() * JPH::Vec3(0, 0, 1);
    return glm::degrees(std::atan2(forward.GetZ(), forward.GetX()));
}

} // namespace forge::phys
