#include "Snowflakes/snowflakes.hpp"
#include "main.hpp"
#include "assets.hpp"
#include "logging.hpp"
#include "Snowflakes/particleController.hpp"

#include "UnityEngine/Resources.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/ParticleSystemRenderer.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/BoxCollider.hpp"
#include "UnityEngine/Collider.hpp"
#include "custom-types/shared/macros.hpp"

#include "bsml/shared/Helpers/utilities.hpp"
#include <cmath>
#include <cstring>

using namespace GlobalNamespace;
using namespace UnityEngine;
using namespace Flair;

namespace Christmas::snowflakes {
    void UpdateWind();
}

DECLARE_CLASS_CODEGEN(Christmas, SnowWindController, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, Update);
};
DEFINE_TYPE(Christmas, SnowWindController);

namespace Christmas::snowflakes {
    namespace {
        SafePtrUnity<GameObject> dustGo;
        SafePtrUnity<GameObject> fineGo;
        SafePtrUnity<GameObject> mediumGo;
        SafePtrUnity<GameObject> snowflakesGo;
        SafePtrUnity<GameObject> collisionFloor;
        float nextWindUpdate = 0.0f;

        // Physics colliders are required for World collision; the menu
        // floor is often visual-only. Keep this invisible static collider
        // separate from UI hierarchy and delete it on teardown.
        void EnsureSnowGround() {
            if (collisionFloor) return;
            auto* ground = GameObject::New_ctor("ChristmasSnowGroundCollision");
            if (!ground) { INFO("[SnowCollision] Ground proxy creation failed"); return; }
            ground->transform->position = Vector3(0.0f, -0.18f, 0.0f);
            auto* collider = ground->AddComponent<BoxCollider*>();
            if (!collider) {
                INFO("[SnowCollision] Ground BoxCollider creation failed");
                GameObject::Destroy(ground);
                return;
            }
            collider->set_size(Vector3(24.0f, 0.36f, 24.0f));
            collider->set_isTrigger(false);
            ground->layer = 0;
            collisionFloor = ground;
            INFO("[SnowCollision] Invisible static ground proxy installed y=0, span=24");
        }

        void LogSceneColliders() {
            int total = 0;
            int active = 0;
            for (auto* collider : Resources::FindObjectsOfTypeAll<Collider*>()) {
                if (!collider) continue;
                ++total;
                if (collider->gameObject && collider->gameObject->get_activeInHierarchy() && collider->get_enabled()) ++active;
            }
            INFO("[SnowCollision] Physics colliders discovered: total={}, active={}", total, active);
        }

        // Limits are per emitter, not merely approximate emission targets.
        // Visual weighting is 55% fine, 30% medium, 15% original crystal.
        ParticleSystem* SpawnLayer(const char* name, float height,
                                   float lifetime, float size, float rate,
                                   int limit, float gravity, bool collide) {
            auto root = GameObject::Instantiate(dustGo->gameObject);
            if (!root) return nullptr;
            root->name = name;
            root->transform->parent = nullptr;
            root->transform->position = Vector3(0.0f, height, 0.0f);
            root->transform->rotation = Quaternion::Euler(Vector3(0.0f, 0.0f, 0.0f));
            root->active = true;

            auto* ps = root->GetComponent<ParticleSystem*>();
            if (!ps) { GameObject::Destroy(root); return nullptr; }

            ParticleController::MainModule::set_startLifetime(ps, lifetime);
            ParticleController::MainModule::set_startSize(ps, size);
            ParticleController::MainModule::set_startSpeed(ps, 0.0f);
            ParticleController::MainModule::set_maxParticles(ps, limit);
            ParticleController::MainModule::set_simulationSpace(ps, ParticleController::SimulationSpace::World);
            ParticleController::MainModule::set_gravityModifier(ps, gravity);
            ParticleController::EmissionModule::set_enabled(ps, true);
            ParticleController::EmissionModule::set_rateOverTime(ps, rate);
            ParticleController::ShapeModule::set_shapeType(ps, ParticleController::ShapeType::Box);
            ParticleController::ShapeModule::set_position(ps, Vector3(0.0f, 0.0f, 0.0f));
            ParticleController::ShapeModule::set_rotation(ps, Vector3(0.0f, 0.0f, 0.0f));
            ParticleController::ShapeModule::set_scale(ps, Vector3(20.0f, 1.0f, 20.0f));

            // Collisions on thousands of tiny flakes are expensive in VR.
            ParticleController::CollisionModule::set_enabled(ps, collide);
            if (collide) {
                // Restore world/ground collisions only for medium and crystal flakes.
                ParticleController::CollisionModule::set_type(
                    ps, ParticleController::CollisionType::World);
                // The original theme did not override collision mode.
                ParticleController::CollisionModule::set_collidesWith(
                    ps, UnityEngine::LayerMask(0xffffffff));
                ParticleController::CollisionModule::set_dampen(ps, 0.08f);
                ParticleController::CollisionModule::set_bounce(ps, 0.0f);
                ParticleController::CollisionModule::set_lifetimeLoss(ps, 0.02f);
                // Preserve DustPS inherited dynamic-collider behavior from the original theme.
                // Preserve the original DustPS collision quality and mode.
                // Preserve inherited collision radius and scene interaction.
            }
            ParticleController::VelocityOverLifetimeModule::set_enabled(ps, !collide);
            ParticleController::NoiseModule::set_enabled(ps, true);
            ParticleController::NoiseModule::set_strength(ps, 0.045f);
            ParticleController::NoiseModule::set_frequency(ps, 0.22f);

            if (std::strcmp(name, "ChristmasFineWindSnow") == 0) fineGo = root;
            if (std::strcmp(name, "ChristmasMediumSnow") == 0) mediumGo = root;
            if (std::strcmp(name, "SnowflakePS") == 0) snowflakesGo = root;
            return ps;
        }

        void SetFlow(ParticleSystem* ps, float horizontal, float downward) {
            if (!ps) return;
            ParticleController::VelocityOverLifetimeModule::set_x(ps, horizontal);
            ParticleController::VelocityOverLifetimeModule::set_y(ps, downward);
            ParticleController::VelocityOverLifetimeModule::set_z(ps, 0.0f);
        }
    }

    void CreateSnowflakes() {
        if (snowflakesGo) return;

        if (!dustGo) for (auto particle : Resources::FindObjectsOfTypeAll<ParticleSystem*>()) {
            if (particle && particle->name == "DustPS") {
                dustGo = particle->gameObject;
                break;
            }
        }
        if (!dustGo) { INFO("[SnowBlend] DustPS source unavailable"); return; }

        dustGo->active = false;
        EnsureSnowGround();
        LogSceneColliders();
        auto* fine = SpawnLayer("ChristmasFineWindSnow", 7.3f, 7.0f,
                                0.012f, 850.0f, 6000, 0.012f, false);
        auto* medium = SpawnLayer("ChristmasMediumSnow", 7.0f, 9.0f,
                                  0.029f, 260.0f, 2500, 0.018f, false);
        auto* crystal = SpawnLayer("SnowflakePS", 7.0f, 10.0f,
                                   0.055f, 48.0f, 550, 0.10f, true);
        if (!fine || !medium || !crystal) {
            INFO("[SnowBlend] A layer could not be initialized; remaining snow is retained");
            return;
        }

        // Retain the original five-frame crystalline texture on the sparse
        // decorative layer, without applying it to the two finer layers.
        auto* renderer = snowflakesGo->GetComponent<ParticleSystemRenderer*>();
        if (renderer && renderer->material) {
            renderer->material->mainTexture = BSML::Utilities::LoadTextureRaw(
                static_cast<ArrayW<uint8_t>>(IncludedAssets::snowflakes_png));
            ParticleController::TextureSheetAnimationModule::set_enabled(crystal, true);
            ParticleController::TextureSheetAnimationModule::set_mode(
                crystal, ParticleController::AnimationMode::Grid);
            ParticleController::TextureSheetAnimationModule::set_numTilesX(crystal, 5);
            ParticleController::TextureSheetAnimationModule::set_numTilesY(crystal, 1);
            ParticleController::TextureSheetAnimationModule::set_frameOverTime(
                crystal, ParticleController::MinMaxCurve(0.0f, 5.0f));
            ParticleController::TextureSheetAnimationModule::set_startFrame(
                crystal, ParticleController::MinMaxCurve(0.0f, 5.0f));
            ParticleController::TextureSheetAnimationModule::set_cycleCount(crystal, 0);
        }

        SetFlow(fine, 0.65f, -0.90f);
        // Medium and crystal now use original DustPS gravity-driven motion.
        
        snowflakesGo->AddComponent<Christmas::SnowWindController*>();
        INFO("[SnowBlend] Build51: fine 850/s and medium 260/s collision-free; crystal 48/s original-style gravity/world collision");
    }

    void UpdateWind() {
        if (!fineGo || !mediumGo || !snowflakesGo) return;
        const float now = Time::get_time();
        if (now < nextWindUpdate) return;
        nextWindUpdate = now + 0.35f;

        // Incommensurate slow oscillations prevent a repeating synchronized
        // gust pattern, while the low amplitudes preserve menu readability.
        const float gust = 0.23f * std::sin(now * 0.17f)
                         + 0.11f * std::sin(now * 0.43f + 1.7f)
                         + 0.06f * std::sin(now * 0.083f + 2.4f);
        SetFlow(fineGo->GetComponent<ParticleSystem*>(), 0.65f + gust, -0.90f);
        // Do not override gravity-driven colliding layers each frame.
        
    }

    void DestroySnowflakes() {
        if (fineGo) GameObject::Destroy(fineGo->gameObject);
        if (mediumGo) GameObject::Destroy(mediumGo->gameObject);
        if (snowflakesGo) GameObject::Destroy(snowflakesGo->gameObject);
        if (collisionFloor) GameObject::Destroy(collisionFloor->gameObject);
        fineGo = nullptr;
        mediumGo = nullptr;
        snowflakesGo = nullptr;
        collisionFloor = nullptr;
        nextWindUpdate = 0.0f;
        if (dustGo) dustGo->active = true;
    }
}

void Christmas::SnowWindController::Update() {
    Christmas::snowflakes::UpdateWind();
}
