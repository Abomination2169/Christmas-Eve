#include "main.hpp"


#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/FlickeringNeonSign.hpp"
#include "GlobalNamespace/GameServerLobbyFlowCoordinator.hpp"
#include "GlobalNamespace/MultiplayerModeSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/LightWithIdManager.hpp"
#include "GlobalNamespace/MainMenuViewController.hpp"
#include "GlobalNamespace/MediaAsyncLoader.hpp"

#include "logging.hpp"

#include "Menu/ObjectInstances.hpp"

#include "Audio/player.hpp"

#include "GlobalNamespace/SongPreviewPlayer.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "custom-types/shared/macros.hpp"
#include "custom-types/shared/register.hpp"

#include "UnityEngine/Time.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Bounds.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/MaterialPropertyBlock.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/SceneManagement/SceneManagement.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/SpriteRenderer.hpp"

#include "System/Threading/Tasks/Tasks.hpp"

#include "Block/colorchanger.hpp"
#include "Snowflakes/snowflakes.hpp"
#include "Menu/customlogo.hpp"
#include "Menu/snowprimitives.hpp"
#include "Menu/aurora.hpp"
#include "GlobalNamespace/ResultsViewController.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/FireworkItemController.hpp"
#include "Snowflakes/particleController.hpp"
#include "bsml/shared/Helpers/utilities.hpp"
#include <string>
#include <vector>



#include "c.hpp"

#include "scotland2/shared/modloader.h"


using namespace GlobalNamespace;
using namespace UnityEngine;


static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};
// Stores the ID and version of our mod, and is sent to
// the modloader upon startup


namespace {
    constexpr const char* floorDiagnosticBuild = "floor-renderer-v2";

    void LogFloorColor(const char* phase, int rendererId, int slot,
                       const char* source, const char* property, Color color) {
        INFO("[FloorRendererDiagnostic] phase={} renderer={} slot={} source={} {}=({},{},{},{})",
             phase, rendererId, slot, source, property, color.r, color.g, color.b, color.a);
    }

    void SampleFloorRenderers(const char* phase) {
        auto* block = MaterialPropertyBlock::New_ctor();
        int sampled = 0;
        int candidates = 0;
        for (auto* renderer : Resources::FindObjectsOfTypeAll<MeshRenderer*>()) {
            if (!renderer || !renderer->get_enabled()) continue;
            auto* go = renderer->get_gameObject();
            if (!go || !go->get_activeInHierarchy()) continue;
            const auto bounds = renderer->get_bounds();
            const auto min = bounds.get_min();
            const auto max = bounds.get_max();
            const auto size = bounds.get_size();
            // Include broad meshes near the standing area, including unnamed floor pieces.
            if (min.x > 1.5f || max.x < -1.5f || min.z > 2.0f || max.z < -2.0f ||
                min.y > 0.4f || max.y < -0.6f || size.x < 0.3f || size.z < 0.3f) continue;
            const auto name = static_cast<std::string>(go->get_name());
            if (name.starts_with("Christmas") || name.starts_with("Snow")) continue;
            ++candidates;
            if (sampled >= 48) continue;
            ++sampled;
            const int id = renderer->GetInstanceID();
            const auto materials = renderer->get_sharedMaterials();
            INFO("[FloorRendererDiagnostic] phase={} renderer={} name={} y=[{},{}] size=({},{},{}) slots={}",
                 phase, id, name, min.y, max.y, size.x, size.y, size.z, materials.size());

            renderer->GetPropertyBlock(block);
            INFO("[FloorRendererDiagnostic] phase={} renderer={} rendererBlockEmpty={}",
                 phase, id, block->get_isEmpty());
            constexpr const char* properties[] = {"_Color", "_BaseColor", "_EmissionColor", "_TintColor"};
            for (const auto* property : properties) {
                const int propertyId = Shader::PropertyToID(property);
                if (block->HasColor(propertyId))
                    LogFloorColor(phase, id, -1, "renderer-block", property, block->GetColor(propertyId));
            }
            for (int slot = 0; slot < materials.size(); ++slot) {
                auto* material = materials[slot];
                if (!material) continue;
                auto* shader = material->get_shader();
                INFO("[FloorRendererDiagnostic] phase={} renderer={} slot={} material={} shader={}",
                     phase, id, slot, static_cast<std::string>(material->get_name()),
                     shader ? static_cast<std::string>(shader->get_name()) : std::string{});
                renderer->GetPropertyBlock(block, slot);
                for (const auto* property : properties) {
                    const int propertyId = Shader::PropertyToID(property);
                    if (material->HasProperty(propertyId))
                        LogFloorColor(phase, id, slot, "shared-material", property, material->GetColor(propertyId));
                    if (block->HasColor(propertyId))
                        LogFloorColor(phase, id, slot, "slot-block", property, block->GetColor(propertyId));
                }
            }
        }
        INFO("[FloorRendererDiagnostic] build={} phase={} sampled={} candidates={}",
             floorDiagnosticBuild, phase, sampled, candidates);
    }
}

DECLARE_CLASS_CODEGEN(Christmas, FloorRendererSample, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(float, sampleAt);
    DECLARE_INSTANCE_FIELD(int, phase);
    DECLARE_INSTANCE_METHOD(void, Update);
};
DEFINE_TYPE(Christmas, FloorRendererSample);

void Christmas::FloorRendererSample::Update() {
    if (Time::get_realtimeSinceStartup() < sampleAt) return;
    SampleFloorRenderers(phase == 0 ? "startup-menu" : phase == 1 ? "results" : "after-results");
    GameObject::Destroy(get_gameObject());
}

namespace {
    void ScheduleFloorSample(int phase) {
        auto* probe = GameObject::New_ctor("ChristmasFloorRendererDiagnostic");
        auto* sampler = probe->AddComponent<Christmas::FloorRendererSample*>();
        if (!sampler) {
            GameObject::Destroy(probe);
            INFO("[FloorRendererDiagnostic] Failed to schedule phase={}", phase);
            return;
        }
        sampler->phase = phase;
        sampler->sampleAt = Time::get_realtimeSinceStartup() + 0.5f;
    }
}

// Called whenever the main menu scene loads
MAKE_HOOK_MATCH(SongPreviewPlayer_OnEnable, &SongPreviewPlayer::OnEnable, void, SongPreviewPlayer* self) {
    if(getModConfig().EnableMenuMusic.GetValue()) PlayRandomSong(self);
    SongPreviewPlayer_OnEnable(self);
}

// Called whenever the SongPreviewPlayer transitions from one song to another
MAKE_HOOK_MATCH(SongPreviewPlayer_CrossfadeTo, static_cast<void(SongPreviewPlayer::*)(AudioClip*, float, float, float, bool, System::Action*)>(&SongPreviewPlayer::CrossfadeTo), void, SongPreviewPlayer* self, AudioClip* audioClip, float musicVolume, float startTime, float duration, bool isDefault, System::Action* onFadeOutCallback) {
    // Menu crossfades are normal navigation events, not requests for a new song.
    // Retain the selected Christmas track across menu transitions.
    SongPreviewPlayer_CrossfadeTo(self, audioClip, musicVolume, startTime, duration, isDefault, onFadeOutCallback);
}

MAKE_HOOK_MATCH(FestiveLogoInit, &GlobalNamespace::FlickeringNeonSign::Start, void, GlobalNamespace::FlickeringNeonSign* self) {
    FestiveLogoInit(self);

    if(getModConfig().EnableCustomLogo.GetValue()) Christmas::customLogo::ApplyFestiveLogoColors();
    // Build 77: retry preview when the actual neon anchor comes into existence.
    if (UnityEngine::SceneManagement::SceneManager::GetActiveScene().name != "GameCore")
        Christmas::aurora::PreviewMenu();
}

// Very quick and dirty way to override the menu light colors
MAKE_HOOK_MATCH(OverrideEnvironmentColors, &GlobalNamespace::LightWithIdManager::SetColorForId, void, GlobalNamespace::LightWithIdManager* self, int lightId, UnityEngine::Color color) {
    const bool gameplay = UnityEngine::SceneManagement::SceneManager::GetActiveScene().name == "GameCore";
    // Diagnostic only: record the first environment-light update in each scene mode.
    // This hook does not establish which renderer owns the player floor.
    static int previousMode = -1;
    const int currentMode = gameplay ? 1 : 0;
    if (previousMode != currentMode) {
        INFO("[FloorColorDiagnostic] LightWithIdManager transition mode={} lightId={} whiteEnv={} incomingRGBA=({},{},{},{})",
             gameplay ? "GameCore" : "Menu", lightId, getModConfig().EnableWhiteEnv.GetValue(),
             color.r, color.g, color.b, color.a);
        previousMode = currentMode;
    }
    if(!getModConfig().EnableWhiteEnv.GetValue() || gameplay)
        OverrideEnvironmentColors(self, lightId, color);
    else {
        // Force the environment color to be white
        OverrideEnvironmentColors(self, lightId, UnityEngine::Color(1.0f, 1.0f, 1.0f, color.a));
    }
}

MAKE_HOOK_MATCH(SnowInit, &GlobalNamespace::MainMenuViewController::DidActivate, void, GlobalNamespace::MainMenuViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    SnowInit(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if(!firstActivation) return;
    ScheduleFloorSample(0);

    if(getModConfig().EnableSnowflakes.GetValue()) Christmas::snowflakes::CreateSnowflakes();
    if(getModConfig().EnableSnowPiles.GetValue()) Christmas::snowPrimitives::CreateSnowPrimitives();
    Christmas::aurora::PreviewMenu();
}


// Fireworks are initialized before ResultsViewController::DidActivate in 1.40.8.
// Retain safe Unity handles, not raw pointers, until the next initialization.
namespace {
    std::vector<SafePtrUnity<GlobalNamespace::FireworkItemController>> resultFireworks;
    bool christmasResultsEligible = false;

    void SetFireworkEmission(bool enabled) {
        for (auto& firework : resultFireworks) {
            if (!firework) continue; // Unity may have destroyed the previous scene.
            auto* controller = firework.ptr();
            if (!controller->_particleSystems) continue;
            for (auto* entry : controller->_particleSystems) {
                if (!entry || !entry->_particleSystem) continue;
                Flair::ParticleController::EmissionModule::set_enabled(
                    entry->_particleSystem.ptr(), enabled);
            }
        }
    }
}

MAKE_HOOK_MATCH(ChristmasFireworkInit, &GlobalNamespace::FireworkItemController::InitializeParticleSystem, void,
    GlobalNamespace::FireworkItemController* self) {
    ChristmasFireworkInit(self);
    if (!self) return;
    // Re-initialization is the authoritative reset. Never disable particles
    // before the results rank is known; an A or failed run keeps vanilla effects.
    bool alreadyTracked = false;
    for (auto& entry : resultFireworks) {
        if (entry && entry.ptr() == self) { alreadyTracked = true; break; }
    }
    if (!alreadyTracked) resultFireworks.emplace_back(self);
    if (christmasResultsEligible) SetFireworkEmission(false);
}

MAKE_HOOK_MATCH(ChristmasFireworkAudio, &GlobalNamespace::FireworkItemController::PlayExplosionSound, void,
    GlobalNamespace::FireworkItemController* self) {
    if (christmasResultsEligible) return; // Keep the one-shot Christmas bell.
    ChristmasFireworkAudio(self);
}

// The firework controller itself owns the results-screen bloom-light flashes.
// Suppress only its SetLightsColor updates while an S+ aurora is active.
// Do not intercept global environment lights or gameplay lighting.
MAKE_HOOK_MATCH(ChristmasFireworkLights, &GlobalNamespace::FireworkItemController::SetLightsColor, void,
    GlobalNamespace::FireworkItemController* self, float intensity) {
    ChristmasFireworkLights(self, christmasResultsEligible ? 0.0f : intensity);
}

// Halloween Theme's Quest 1.40.8 results-rank detection:
// RankModel::Rank is E=0, D=1, C=2, B=3, A=4, S=5, SS=6, SSS=7.
MAKE_HOOK_MATCH(ChristmasEveResultsActivate, &GlobalNamespace::ResultsViewController::DidActivate, void, GlobalNamespace::ResultsViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    ChristmasEveResultsActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);
    if (addedToHierarchy) ScheduleFloorSample(1);
    auto* completed = self ? self->__cordl_internal_get__levelCompletionResults() : nullptr;
    const bool rankSOrAbove = completed && completed->__cordl_internal_get_rank().value__ >= 5;
    christmasResultsEligible = rankSOrAbove;
    INFO("[AuroraSRank] Results activated first={} added={} eligible={} fireworkControllers={}",
         firstActivation, addedToHierarchy, rankSOrAbove, resultFireworks.size());
    if (rankSOrAbove) {
        SetFireworkEmission(false);
        Christmas::aurora::BeginResults();
    }
}

MAKE_HOOK_MATCH(ChristmasEveResultsDeactivate, &GlobalNamespace::ResultsViewController::DidDeactivate, void, GlobalNamespace::ResultsViewController* self, bool removedFromHierarchy, bool screenSystemDisabling) {
    Christmas::aurora::EndResults();
    // Return the vanilla emission state when leaving the eligible results UI.
    if (christmasResultsEligible) SetFireworkEmission(true);
    christmasResultsEligible = false;
    ChristmasEveResultsDeactivate(self, removedFromHierarchy, screenSystemDisabling);
    if (removedFromHierarchy) ScheduleFloorSample(2);
}

// Called at the early stages of game loading
MOD_EXTERN_FUNC void setup(CModInfo *info) noexcept {
    *info = modInfo.to_c();

    getModConfig().Init(modInfo);

    // File logging
    Paper::Logger::RegisterFileContextId(PaperLogger.tag);

    INFO("[FloorRendererDiagnostic] build={} startup", floorDiagnosticBuild);
    INFO("Completed setup!");
}

// Called later on in the game loading - a good time to install function hooks
MOD_EXTERN_FUNC void late_load() noexcept {
    il2cpp_functions::Init();
    custom_types::Register::AutoRegister();
    INFO("[Garland] Registered custom sparkle MonoBehaviour");


    INFO("Installing hooks...");
    INSTALL_HOOK(PaperLogger, FestiveLogoInit);
    INSTALL_HOOK(PaperLogger, OverrideEnvironmentColors);
    INSTALL_HOOK(PaperLogger, SnowInit);
    INSTALL_HOOK(PaperLogger, ChristmasEveResultsActivate);
    INSTALL_HOOK(PaperLogger, ChristmasEveResultsDeactivate);
    INSTALL_HOOK(PaperLogger, ChristmasFireworkInit);
    INSTALL_HOOK(PaperLogger, ChristmasFireworkAudio);
    INSTALL_HOOK(PaperLogger, ChristmasFireworkLights);

    INSTALL_HOOK(PaperLogger, SongPreviewPlayer_OnEnable);
    INSTALL_HOOK(PaperLogger, SongPreviewPlayer_CrossfadeTo);



    INFO("Installed all hooks!");
}
