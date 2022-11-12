#include "input.hpp"
#include "../debug/led.hpp"
#include <PDM.h>

void mic::on_PDM_data() // using raw function pointers so can't use lambda
{ // this isn't c++03 is it?
    __decltype(1) bytes_available = PDM.available();
    PDM.read(buffer.data(), bytes_available);
    samples_read = bytes_available / sizeof(short);
}

void mic::init()
{
    if (initialized)
        return;
    initialized = true;
    PDM.onReceive(mic::on_PDM_data); // using raw function pointers ewww

    if (!PDM.begin(channels, frequency))
    {
        rgb_led::error(36000000ul);
    }
}

bool mic::initialized = false;
__decltype(1) mic::frequency = 16000;
__decltype(1) mic::channels = 1;
std::array<short, mic::BUFFER_SIZE> mic::buffer;
volatile __decltype(1) mic::samples_read = 0;

