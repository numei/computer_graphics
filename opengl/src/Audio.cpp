#include "Audio.h"
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h" // put dr_wav.h into src/
#include <iostream>
#include <vector>
#include <cstdint>
// Undefine Windows PlaySound macro if it exists (from windows.h)
#ifdef PlaySound
#undef PlaySound
#endif

static ALCdevice *device = nullptr;
static ALCcontext *context = nullptr;

bool Audio::Init()
{
    device = alcOpenDevice(nullptr);
    if (!device)
        return false;
    context = alcCreateContext(device, nullptr);
    alcMakeContextCurrent(context);
    return true;
}

void Audio::Shutdown()
{
    alcMakeContextCurrent(nullptr);
    if (context)
        alcDestroyContext(context);
    if (device)
        alcCloseDevice(device);
}

unsigned int Audio::LoadWAV(const std::string &path)
{
    drwav wav;
    if (!drwav_init_file(&wav, path.c_str(), NULL))
    {
        std::cerr << "Failed to open wav: " << path << "\n";
        return 0;
    }
    size_t samples = wav.totalPCMFrameCount * wav.channels;
    std::vector<int16_t> pcm(samples);
    drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, pcm.data());
    ALenum format = (wav.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    ALuint buf;
    alGenBuffers(1, &buf);
    alBufferData(buf, format, pcm.data(), (ALsizei)(pcm.size() * sizeof(int16_t)), wav.sampleRate);
    drwav_uninit(&wav);
    return buf;
}

unsigned int Audio::PlaySound(unsigned int buffer, bool loop)
{
    if (!buffer)
        return 0;
    ALuint src;
    alGenSources(1, &src);
    alSourcei(src, AL_BUFFER, buffer);
    alSourcei(src, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    alSourcePlay(src);
    return src;
}

void Audio::Stop(unsigned int source)
{
    if (source)
    {
        alSourceStop(source);
        alDeleteSources(1, &source);
    }
}
