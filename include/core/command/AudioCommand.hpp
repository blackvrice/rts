//
// Created by black on 26. 6. 27..
//
#pragma once
#include <memory>
#include <string>
#include "Command.hpp"

namespace rts::core::manager {
    class IAudioManager;
}

namespace rts::core::command {
    class AudioCommand : public Command  {
        public:
            ~AudioCommand() override = default;
    };

    using AudioCommandPtr = std::unique_ptr<AudioCommand>;

    // =========================================================
    // Playback
    // =========================================================
    // Fire-and-forget sound effect. `name` is a path relative to the audio asset
    // root (e.g. "sfx/select.wav"); `volume` is 0..100 before the master scale.
    class PlaySoundCommand final : public AudioCommand {
    public:
        explicit PlaySoundCommand(std::string name, float volume = 100.0f)
            : m_name(std::move(name)), m_volume(volume) {
        }

        const std::string &name() const noexcept { return m_name; }
        float volume() const noexcept { return m_volume; }

    private:
        std::string m_name;
        float m_volume;
    };

    // Streams a background track (replacing any current one). Loops by default.
    class PlayMusicCommand final : public AudioCommand {
    public:
        explicit PlayMusicCommand(std::string name, bool loop = true, float volume = 100.0f)
            : m_name(std::move(name)), m_loop(loop), m_volume(volume) {
        }

        const std::string &name() const noexcept { return m_name; }
        bool loop() const noexcept { return m_loop; }
        float volume() const noexcept { return m_volume; }

    private:
        std::string m_name;
        bool m_loop;
        float m_volume;
    };

    // Plays a semantic gameplay cue (e.g. "select", "attack", "death"). The SFML
    // backend synthesizes a one-shot tone per cue key, so no asset is required.
    // This is how gameplay feedback (SoundCue) reaches the audio thread.
    class PlayCueCommand final : public AudioCommand {
    public:
        explicit PlayCueCommand(std::string cue, float volume = 70.0f)
            : m_cue(std::move(cue)), m_volume(volume) {
        }

        const std::string &cue() const noexcept { return m_cue; }
        float volume() const noexcept { return m_volume; }

    private:
        std::string m_cue;
        float m_volume;
    };

    class StopMusicCommand final : public AudioCommand {
    };

    // Master gain applied to every voice and the music stream (0..100).
    class SetMasterVolumeCommand final : public AudioCommand {
    public:
        explicit SetMasterVolumeCommand(float volume) : m_volume(volume) {
        }

        float volume() const noexcept { return m_volume; }

    private:
        float m_volume;
    };

    // Installs (or swaps) the manager the AudioThread drives. Mirrors
    // ChangeLogicManagerCommand so scene changes can repoint audio off-thread.
    class ChangeAudioManagerCommand final : public AudioCommand {
    public:
        explicit ChangeAudioManagerCommand(
            std::shared_ptr<manager::IAudioManager> audioManager)
            : m_audioManager(std::move(audioManager)) {
        }

        const std::shared_ptr<manager::IAudioManager> &audio() const {
            return m_audioManager;
        }

    private:
        std::shared_ptr<manager::IAudioManager> m_audioManager;
    };
}
