#include "Audio/player.hpp"
#include "main.hpp"
#include "GlobalNamespace/MediaAsyncLoader.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "System/Threading/Tasks/Tasks.hpp"
#include "custom-types/shared/delegate.hpp"
#include <random>
#include <array>

// Three recordings are eligible for private testing only; see MUSIC_LICENSES.md for unverified redistribution rights.
static const std::array<const char*, 3> songPathList = {{
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/Christmas/we_wish_you_a_merry_christmas.ogg",
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/Christmas/dance_of_the_sugar_plum_fairy.ogg",
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/Christmas/M668_Christmas_Lights_5m22_click_removed.ogg"
}};

void PlayRandomSong(GlobalNamespace::SongPreviewPlayer* songPreviewPlayer) {
    if (!songPreviewPlayer) return;
    static std::mt19937 gen(std::random_device{}());
    // Keep the same menu track for the lifetime of this game session.
    // Menu reactivation must not reshuffle the music.
    static const std::size_t selectedIndex = [&]() {
        std::uniform_int_distribution<int> dis(0, static_cast<int>(songPathList.size()) - 1);
        return static_cast<std::size_t>(dis(gen));
    }();
    const std::string path(songPathList[selectedIndex]);
    if (!fileexists(path)) return;

    auto* loader = GlobalNamespace::MediaAsyncLoader::New_ctor();
    if (!loader) return;
    auto* task = loader->LoadAudioClipFromFilePathAsync(path);
    if (!task) return;

    // The menu can be torn down while this file loads. A Unity-safe handle
    // expires with the player, unlike a captured raw pointer.
    SafePtrUnity<GlobalNamespace::SongPreviewPlayer> player = songPreviewPlayer;
    task->GetAwaiter().OnCompleted(custom_types::MakeDelegate<System::Action*>(
        std::function([player, task]() {
            if (!player || !task || task->get_IsFaulted() || task->get_IsCanceled()) return;
            // Never install menu music during gameplay scene activation.
            if (UnityEngine::SceneManagement::SceneManager::GetActiveScene().name == "GameCore") return;
            auto clip = task->get_Result();
            if (!clip) return;
            player->_defaultAudioClip = clip;
        })
    ));
}
