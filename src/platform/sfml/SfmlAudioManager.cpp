//
// Created by black on 26. 6. 27..
//
#include "platform/sfml/SfmlAudioManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include <SFML/Audio/SoundChannel.hpp>

#include "core/command/AudioCommand.hpp"
#include "core/command/CommandRouterBase.hpp"
#include "platform/sfml/SfmlAssetPaths.hpp"

namespace rts::platform::sfml {

    namespace {
        // SFML caps simultaneous voices (~256). Stay well under so a burst of
        // effects never throws; the oldest still-playing voice is dropped instead.
        constexpr std::size_t kMaxVoices = 64;

        struct ToneSpec {
            float frequency;
            float duration;
            float volumeScale;
        };

        // Per-cue synthesized tone shape (moved verbatim from SfmlRenderManager so
        // the audible feedback is unchanged, only the thread it plays on differs).
        ToneSpec toneForCue(const std::string &cue) {
            if (cue == "select") return { 660.0f, 0.055f, 0.45f };
            if (cue == "move") return { 520.0f, 0.07f, 0.42f };
            if (cue == "attack") return { 220.0f, 0.11f, 0.62f };
            if (cue == "fire") return { 880.0f, 0.045f, 0.40f };
            if (cue == "hit") return { 180.0f, 0.08f, 0.50f };
            if (cue == "death") return { 120.0f, 0.18f, 0.58f };
            if (cue == "production") return { 740.0f, 0.13f, 0.48f };
            if (cue == "construction") return { 420.0f, 0.16f, 0.52f };
            if (cue == "shortage") return { 130.0f, 0.12f, 0.70f };
            if (cue == "gather") return { 590.0f, 0.05f, 0.34f };
            if (cue == "victory") return { 920.0f, 0.22f, 0.62f };
            if (cue == "defeat") return { 90.0f, 0.25f, 0.70f };
            if (cue == "explosion") return { 75.0f, 0.16f, 0.70f };
            return { 440.0f, 0.06f, 0.40f };
        }
    }

    SfmlAudioManager::SfmlAudioManager(core::command::AudioCommandBus &bus,
                                       core::command::AudioCommandRouter &router)
        : IAudioManager(bus, router),
          m_audioRoot(AudioRoot) {
        registerHandlers();
    }

    void SfmlAudioManager::registerHandlers() {
        m_router.on<core::command::PlaySoundCommand>(
            [this](const core::command::PlaySoundCommand &cmd) {
                playSound(cmd.name(), cmd.volume());
            });
        m_router.on<core::command::PlayMusicCommand>(
            [this](const core::command::PlayMusicCommand &cmd) {
                playMusic(cmd.name(), cmd.loop(), cmd.volume());
            });
        m_router.on<core::command::StopMusicCommand>(
            [this](const core::command::StopMusicCommand &) {
                stopMusic();
            });
        m_router.on<core::command::SetMasterVolumeCommand>(
            [this](const core::command::SetMasterVolumeCommand &cmd) {
                setMasterVolume(cmd.volume());
            });
        m_router.on<core::command::PlayCueCommand>(
            [this](const core::command::PlayCueCommand &cmd) {
                playCue(cmd.cue(), cmd.volume());
            });
    }

    void SfmlAudioManager::update() {
        m_voices.remove_if([](const ::sf::Sound &voice) {
            return voice.getStatus() == ::sf::Sound::Status::Stopped;
        });
    }

    void SfmlAudioManager::tick(float /*dt*/) {
        // No time-based audio work yet (reserved for fades / ducking).
    }

    float SfmlAudioManager::scaledVolume(float volume) const {
        return std::clamp(volume, 0.0f, 100.0f) * (m_masterVolume / 100.0f);
    }

    const ::sf::SoundBuffer *SfmlAudioManager::buffer(const std::string &relName) {
        if (const auto it = m_buffers.find(relName); it != m_buffers.end()) {
            return it->second ? &*it->second : nullptr;
        }

        const std::filesystem::path path = m_audioRoot / relName;
        ::sf::SoundBuffer buf;
        if (!buf.loadFromFile(path.string())) {
            // Cache the miss so a missing file is not retried on every play.
            m_buffers.emplace(relName, std::nullopt);
            return nullptr;
        }

        auto [it, _] = m_buffers.emplace(relName, std::move(buf));
        return &*it->second;
    }

    const ::sf::SoundBuffer &SfmlAudioManager::cueBuffer(const std::string &cue) {
        if (const auto it = m_cueBuffers.find(cue); it != m_cueBuffers.end()) {
            return it->second;
        }

        constexpr unsigned int kSampleRate = 44100;
        const ToneSpec spec = toneForCue(cue);
        const auto sampleCount = static_cast<std::uint64_t>(kSampleRate * spec.duration);
        std::vector<std::int16_t> samples(static_cast<std::size_t>(sampleCount));
        for (std::uint64_t i = 0; i < sampleCount; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
            const float envelope = 1.0f - static_cast<float>(i) / static_cast<float>(sampleCount);
            const float wave = std::sin(2.0f * std::numbers::pi_v<float> * spec.frequency * t);
            samples[static_cast<std::size_t>(i)] =
                static_cast<std::int16_t>(wave * envelope * spec.volumeScale * 30000.0f);
        }

        ::sf::SoundBuffer buffer;
        (void)buffer.loadFromSamples(
            samples.data(), sampleCount, 1, kSampleRate, { ::sf::SoundChannel::Mono });
        auto [it, _] = m_cueBuffers.emplace(cue, std::move(buffer));
        return it->second;
    }

    void SfmlAudioManager::playCue(const std::string &cue, float volume) {
        const ::sf::SoundBuffer &buf = cueBuffer(cue);

        if (m_voices.size() >= kMaxVoices) {
            m_voices.pop_front();
        }

        ::sf::Sound &voice = m_voices.emplace_back(buf);
        voice.setVolume(scaledVolume(volume));
        voice.play();
    }

    void SfmlAudioManager::playSound(const std::string &relName, float volume) {
        const ::sf::SoundBuffer *buf = buffer(relName);
        if (!buf) return;

        if (m_voices.size() >= kMaxVoices) {
            m_voices.pop_front();
        }

        ::sf::Sound &voice = m_voices.emplace_back(*buf);
        voice.setVolume(scaledVolume(volume));
        voice.play();
    }

    void SfmlAudioManager::playMusic(const std::string &relName, bool loop, float volume) {
        const std::filesystem::path path = m_audioRoot / relName;
        if (!m_music.openFromFile(path.string())) {
            m_musicOpen = false;
            return;
        }

        m_musicOpen = true;
        m_musicVolume = std::clamp(volume, 0.0f, 100.0f);
        m_music.setLooping(loop);
        m_music.setVolume(scaledVolume(m_musicVolume));
        m_music.play();
    }

    void SfmlAudioManager::stopMusic() {
        if (m_musicOpen) {
            m_music.stop();
        }
    }

    void SfmlAudioManager::setMasterVolume(float volume) {
        m_masterVolume = std::clamp(volume, 0.0f, 100.0f);
        // Re-scale the live music stream; active one-shots keep their launch volume.
        if (m_musicOpen) {
            m_music.setVolume(scaledVolume(m_musicVolume));
        }
    }

} // namespace rts::platform::sfml
