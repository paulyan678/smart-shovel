#ifndef AUDIO_INPUT_HPP
#define AUDIO_INPUT_HPP
#include <array>
#include "../std/frame.hpp"

class mic
{
public:
    static constexpr size_t BUFFER_SIZE = 512;
    static __decltype(1) frequency;
    static void on_PDM_data();
    mic() = delete;

public:
    static void init();

    template<typename T>
    static bool read(frame <T> &buf)
    {
        if (!samples_read)
            return false;
        for (__decltype(1) i = 0; i < samples_read; i++)
            buf.push_back(buffer[i]);

        samples_read = 0;
        return true;
    }

private:
    static std::array<short, BUFFER_SIZE> buffer;
    static bool initialized;
    static volatile __decltype(1) samples_read;
    static __decltype(1) channels;

};

#endif
