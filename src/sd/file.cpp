#include "file.hpp"

void sd::init(uint8_t chip_select)
{
    // see if the card is present and can be initialized:
    if (!SD.begin(chip_select)) {
        // don't do anything more:
        rgb_led::error(errorcode::SD_INIT);
    }
    sd_file::initialized = true;
}

sd_file::sd_file(const char *filename, uint8_t mode)
{
    if (opened)
    {
        rgb_led::error(errorcode::SD_OPEN);
    }
    file_intern = SD.open(filename, mode);
    if (!file_intern)
    {
        rgb_led::error(errorcode::SD_OPEN);
    }
    opened = true;
}

sd_file::~sd_file() noexcept
{
    close();
}

void sd_file::close() noexcept
{
    if (opened)
    {
        file_intern.close();
        opened = false;
    }
}

bool sd_file::opened = false;

bool sd_file::initialized = false;
