#include "engine/phys/world.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/RegisterTypes.h>

#include <glm/glm.hpp>
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

} // namespace

struct World::Impl {
    Broadphase broadphase;
    ObjectVsBroadphase object_vs_bp;
    ObjectPair object_pair;
    std::unique_ptr<JPH::TempAllocatorImpl> allocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobs;
    JPH::PhysicsSystem system;
    JPH::BodyInterface* bodies = nullptr;
    JPH::Ref<JPH::CharacterVirtual> character;
    JPH::RefConst<JPH::Shape> character_shape;
    JPH::Body* car = nullptr;
    JPH::Ref<JPH::VehicleConstraint> vehicle;
    JPH::RefConst<JPH::VehicleCollisionTester> tester;
    float vertical_velocity = 0.0f;
    float vehicle_forward = 0.0f;
    float vehicle_steer = 0.0f;
    float vehicle_brake = 0.0f;
    bool character_enabled = true;
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
    impl_->character = nullptr;
    impl_->character_shape = nullptr;
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
    impl_->system.SetGravity({0, -9.81f, 0});
    impl_->bodies = &impl_->system.GetBodyInterface();
    return true;
}

void World::add_box(glm::vec3 center, glm::vec3 half_extents)
{
    auto shape = new JPH::BoxShape(to_jolt(half_extents));
    JPH::BodyCreationSettings settings(shape, to_jolt(center), JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                                       layer_static);
    impl_->bodies->CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}

void World::spawn_character(glm::vec3 position, float radius, float half_height)
{
    impl_->character_shape = new JPH::CapsuleShape(half_height, radius);
    JPH::CharacterVirtualSettings settings;
    settings.mShape = impl_->character_shape;
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);
    settings.mMass = 80.0f;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);
    impl_->character = new JPH::CharacterVirtual(&settings, to_jolt(position), JPH::Quat::sIdentity(), 0,
                                                 &impl_->system);
    impl_->vertical_velocity = 0.0f;
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

void World::set_character_enabled(bool enabled) { impl_->character_enabled = enabled; }

void World::warp_character(glm::vec3 position)
{
    if (!impl_->character) return;
    impl_->character->SetPosition(to_jolt(position));
    impl_->vertical_velocity = 0.0f;
}

void World::set_vehicle_input(float forward, float steer, float brake)
{
    impl_->vehicle_forward = forward;
    impl_->vehicle_steer = steer;
    impl_->vehicle_brake = brake;
}

void World::tick(float dt, glm::vec3 walk_xz, bool jump)
{
    if (impl_->vehicle && impl_->car) {
        auto* controller = static_cast<JPH::WheeledVehicleController*>(impl_->vehicle->GetController());
        controller->SetDriverInput(impl_->vehicle_forward, impl_->vehicle_steer, impl_->vehicle_brake, 0.0f);
        if (std::abs(impl_->vehicle_forward) + std::abs(impl_->vehicle_steer) + impl_->vehicle_brake > 0.01f)
            impl_->bodies->ActivateBody(impl_->car->GetID());
    }
    if (impl_->character && impl_->character_enabled) {
        const bool grounded = impl_->character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround
            || impl_->character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnSteepGround;
        if (grounded) impl_->vertical_velocity = 0.0f;
        else impl_->vertical_velocity += -9.81f * dt;
        if (jump && grounded) impl_->vertical_velocity = 6.0f;
        const float speed = glm::length(glm::vec2(walk_xz.x, walk_xz.z));
        glm::vec3 walk = walk_xz;
        if (speed > 1.0f) walk /= speed;
        walk *= 5.5f;
        impl_->character->SetLinearVelocity({walk.x, impl_->vertical_velocity, walk.z});
        JPH::CharacterVirtual::ExtendedUpdateSettings update;
        impl_->character->ExtendedUpdate(dt, impl_->system.GetGravity(), update,
                                         impl_->system.GetDefaultBroadPhaseLayerFilter(layer_moving),
                                         impl_->system.GetDefaultLayerFilter(layer_moving), {}, {}, *impl_->allocator);
        impl_->vertical_velocity = impl_->character->GetLinearVelocity().GetY();
    }
    impl_->system.Update(dt, 1, impl_->allocator.get(), impl_->jobs.get());
}

glm::vec3 World::character_position() const
{
    if (!impl_->character) return {};
    return to_glm(impl_->character->GetPosition());
}

bool World::character_supported() const
{
    if (!impl_->character) return false;
    return impl_->character->GetGroundState() != JPH::CharacterVirtual::EGroundState::InAir;
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
