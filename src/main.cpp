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
    // Restrict the white lighting override to the actual menu scene.
    // Results and failure screens may be active outside GameCore; recoloring
    // their light IDs can tint the nearby floor after any song ends.
    const bool mainMenu = UnityEngine::SceneManagement::SceneManager::GetActiveScene().name == "MainMenu";
    if(!getModConfig().EnableWhiteEnv.GetValue() || !mainMenu)
        OverrideEnvironmentColors(self, lightId, color);
    else {
        // Force the environment color to be white
        OverrideEnvironmentColors(self, lightId, UnityEngine::Color(1.0f, 1.0f, 1.0f, color.a));
    }
}

MAKE_HOOK_MATCH(SnowInit, &GlobalNamespace::MainMenuViewController::DidActivate, void, GlobalNamespace::MainMenuViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    SnowInit(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    // Reassert our private snow color whenever the menu reappears after a song.
    Christmas::snowPrimitives::RestoreSnowMaterialColor();
    if(!firstActivation) return;

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
    Christmas::snowPrimitives::RestoreSnowMaterialColor();
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
}

// Called at the early stages of game loading
MOD_EXTERN_FUNC void setup(CModInfo *info) noexcept {
    *info = modInfo.to_c();

    getModConfig().Init(modInfo);

    // File logging
    Paper::Logger::RegisterFileContextId(PaperLogger.tag);

    INFO("Completed setup!");
}

// Called later on in the game loading - a good time to install function hooks
MOD_EXTERN_FUNC void late_load() noexcept {
    il2cpp_functions::Init();
    custom_types::Register::AutoRegister();
    INFO("[Garland] Registered custom sparkle MonoBehaviour");


    INFO("Installing hooks...");
    INSTALL_HOOK(PaperLogger, FestiveLogoInit);
    // Diagnostic build: leave the original White Environment option intact,
    // but do not install the environment color override while investigating
    // blue warning-screen and yellow gameplay/results floor colors.
    // INSTALL_HOOK(PaperLogger, OverrideEnvironmentColors);
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