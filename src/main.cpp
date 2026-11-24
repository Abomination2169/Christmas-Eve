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
#include <vector>



#include "c.hpp"

#include "scotland2/shared/modloader.h"


using namespace GlobalNamespace;
using namespace UnityEngine;


static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};
// Stores the ID and version of our mod, and is sent to
// the modloader upon startup


namespace {
    constexpr const char* floorDiagnosticBuild = "menu-floor-restoration-v3";
    SafePtrUnity<MeshRenderer> menuFloor;
    Color menuFloorBaseline{};
    bool hasMenuFloorBaseline = false;
    bool floorRestorationActive = false;
    bool loggedFloorRestore = false;

    MeshRenderer* FindMenuFloor(int colorProperty) {
        for (auto* renderer : Resources::FindObjectsOfTypeAll<MeshRenderer*>()) {
            if (!renderer || !renderer->get_enabled()) continue;
            auto go = renderer->get_gameObject();
            if (!go || !go->get_activeInHierarchy() || go->get_name() != "BasicMenuGround") continue;
            const auto materials = renderer->get_sharedMaterials();
            if (materials.size() != 1) continue;
            auto material = materials[0];
            if (material && material->get_name() == "MenuFloor" && material->HasProperty(colorProperty))
                return renderer;
        }
        return nullptr;
    }

    void EnableMenuFloorRestoration() {
        floorRestorationActive = true;
        loggedFloorRestore = false;
    }
}

// Unity copies this block into the renderer. Restore the caller's value after
// submission in case the same block is reused for other environment meshes.
MAKE_HOOK_MATCH(MenuFloorSetPropertyBlock,
    static_cast<void (Renderer::*)(MaterialPropertyBlock*)>(&Renderer::SetPropertyBlock),
    void, Renderer* self, MaterialPropertyBlock* block) {
    if (block && hasMenuFloorBaseline && menuFloor &&
        self == static_cast<Renderer*>(menuFloor.ptr()) && getModConfig().EnableWhiteEnv.GetValue()) {
        static const int colorProperty = Shader::PropertyToID("_Color");
        const auto incoming = block->GetColor(colorProperty);
        // Ignore neutral and unset overrides, including the guard's own writes.
        if (incoming.r != incoming.g || incoming.g != incoming.b) {
            block->SetColor(colorProperty, menuFloorBaseline);
            MenuFloorSetPropertyBlock(self, block);
            block->SetColor(colorProperty, incoming);
            if (!loggedFloorRestore) {
                loggedFloorRestore = true;
                INFO("[MenuFloorRestoration] intercepted BasicMenuGround _Color before submission RGBA=({},{},{},{})",
                     incoming.r, incoming.g, incoming.b, incoming.a);
            }
            return;
        }
    }
    MenuFloorSetPropertyBlock(self, block);
}

DECLARE_CLASS_CODEGEN(Christmas, FloorRendererSample, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(float, nextLookupAt);
    DECLARE_INSTANCE_FIELD(UnityEngine::MaterialPropertyBlock*, propertyBlock);
    DECLARE_INSTANCE_METHOD(void, LateUpdate);
};
DEFINE_TYPE(Christmas, FloorRendererSample);

void Christmas::FloorRendererSample::LateUpdate() {
    if (hasMenuFloorBaseline && (!floorRestorationActive || !getModConfig().EnableWhiteEnv.GetValue())) return;
    if (UnityEngine::SceneManagement::SceneManager::GetActiveScene().name == "GameCore") return;

    static const int colorProperty = Shader::PropertyToID("_Color");
    if (!menuFloor) {
        const float now = Time::get_realtimeSinceStartup();
        if (now < nextLookupAt) return;
        nextLookupAt = now + 0.5f;
        menuFloor = FindMenuFloor(colorProperty);
        if (!menuFloor) return;
    }

    auto* renderer = menuFloor.ptr();
    auto go = renderer->get_gameObject();
    if (!go || !go->get_activeInHierarchy() || !renderer->get_enabled()) return;
    renderer->GetPropertyBlock(propertyBlock);
    const auto current = propertyBlock->GetColor(colorProperty);

    if (!hasMenuFloorBaseline) {
        if (floorRestorationActive) return;
        // An unset override is reported as zero by the Quest binding.
        const bool unset = current.r == 0 && current.g == 0 && current.b == 0 && current.a == 0;
        menuFloorBaseline = unset ? renderer->get_sharedMaterials()[0]->GetColor(colorProperty) : current;
        hasMenuFloorBaseline = true;
        INFO("[MenuFloorRestoration] baselineRGBA=({},{},{},{})",
             menuFloorBaseline.r, menuFloorBaseline.g, menuFloorBaseline.b, menuFloorBaseline.a);
        return;
    }

    if (current.r == menuFloorBaseline.r && current.g == menuFloorBaseline.g &&
        current.b == menuFloorBaseline.b && current.a == menuFloorBaseline.a) return;
    // Preserve the other properties written by the menu lighting system.
    propertyBlock->SetColor(colorProperty, menuFloorBaseline);
    renderer->SetPropertyBlock(propertyBlock);
    if (!loggedFloorRestore) {
        loggedFloorRestore = true;
        INFO("[MenuFloorRestoration] LateUpdate corrected BasicMenuGround from RGBA=({},{},{},{})",
             current.r, current.g, current.b, current.a);
    }
}

namespace {
    void StartMenuFloorRestoration() {
        auto* probe = GameObject::New_ctor("ChristmasMenuFloorRestoration");
        auto* sampler = probe->AddComponent<Christmas::FloorRendererSample*>();
        if (!sampler) {
            GameObject::Destroy(probe);
            INFO("[MenuFloorRestoration] Failed to create floor controller");
            return;
        }
        sampler->nextLookupAt = Time::get_realtimeSinceStartup() + 0.5f;
        sampler->propertyBlock = MaterialPropertyBlock::New_ctor();
        GameObject::DontDestroyOnLoad(probe);
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
    StartMenuFloorRestoration();

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
    if (addedToHierarchy) EnableMenuFloorRestoration();
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
    if (removedFromHierarchy) EnableMenuFloorRestoration();
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
    INSTALL_HOOK(PaperLogger, MenuFloorSetPropertyBlock);
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
