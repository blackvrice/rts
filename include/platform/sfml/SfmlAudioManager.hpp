//
// Created by black on 26. 6. 27..
//
#pragma once

#include <filesystem>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

#include "core/manager/IAudioManager.hpp"

namespace rts::platform::sfml {

    // SFML-backed audio manager driven by the AudioThread. It registers handlers on
    // the AudioCommandRouter in its constructor, so playback runs on the audio
    // thread when the thread dispatches drained commands. Sound buffers are loaded
    // lazily and cached; one-shot voices are pooled and reclaimed in update().
    class SfmlAudioManager final : public core::manager::IAudioManager {
    public:
        SfmlAudioManager(core::command::AudioCommandBus &bus,
                         core::command::AudioCommandRouter &router);

        // Drops finished one-shot voices so the active list stays bounded.
        void update() override;
        // Reserved for time-based work (fades); a no-op for now.
        void tick(float dt) override;

    private:
        void registerHandlers();

        // Returns a cached buffer for `relName`, loading it on first use. Returns
        // nullptr (and caches the miss) when the file is absent/unreadable.
        const ::sf::SoundBuffer *buffer(const std::string &relName);

        void playSound(const std::string &relName, float volume);
        void playMusic(const std::string &relName, bool loop, float volume);
        void stopMusic();
        void setMasterVolume(float volume);

        // Plays a synthesized one-shot tone for a semantic cue key ("select", ...).
        // Buffers are generated once per cue and cached. Moved off the main thread
        // from SfmlRenderManager so gameplay feedback plays on the audio thread.
        void playCue(const std::string &cue, float volume);
        const ::sf::SoundBuffer &cueBuffer(const std::string &cue);

        // Scales a per-command 0..100 volume by the master gain.
        float scaledVolume(float volume) const;

        std::filesystem::path m_audioRoot;
        // Cached buffers keyed by relative name. A null entry records a load miss so
        // a missing file is not retried every play.
        std::unordered_map<std::string, std::optional<::sf::SoundBuffer>> m_buffers;
        // Synthesized one-shot tones keyed by cue name (always succeed, so no optional).
        std::unordered_map<std::string, ::sf::SoundBuffer> m_cueBuffers;
        // list (not vector) so active sf::Sound voices are never relocated.
        std::list<::sf::Sound> m_voices;
        ::sf::Music m_music;
        bool m_musicOpen { false };
        float m_musicVolume { 100.0f };   // per-command base, before master scale
        float m_masterVolume { 100.0f };
    };

} // namespace rts::platform::sfml
