#pragma once
#include <string>

// Undefine Windows PlaySound macro if it exists (from windows.h)
// This must be done before declaring the PlaySound function
#ifdef PlaySound
#undef PlaySound
#endif

class Audio
{
public:
    bool Init();
    void Shutdown();
    unsigned int LoadWAV(const std::string &path); // returns buffer id
    unsigned int PlaySound(unsigned int buffer, bool loop = false);
    void Stop(unsigned int source);
};
