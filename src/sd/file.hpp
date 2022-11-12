#ifndef SD_FILE_HPP
#define SD_FILE_HPP
#include <SD.h>
#include <SPI.h>
#include <string>
#include "../debug/led.hpp"
struct sd
{
    sd() = delete;
    static void init(uint8_t chip_select = 10U);
};

class sd_file
{
public:
    friend class sd;

    static constexpr int RETRIES = 32;

    sd_file(const char *filename, uint8_t mode = FILE_WRITE); 
    ~sd_file() noexcept;

    void close() noexcept;

    sd_file &operator<<(const std::string &data)
    {
        return *this << data.c_str();
    }

    sd_file &operator<<(const char *data)
    {
        int tries = 0;
        while (tries++ < RETRIES)
        {
            if (file_intern.availableForWrite())
            {
                file_intern.print(data);
                return *this;
            }
            delay(20L);
        }
        rgb_led::error(errorcode::SD_FAIL);
        return *this;        
    }

    /**
     * @brief Write some data to the file
     * 
     * @tparam T type of the data
     * @param data The data to write
     * @return sd_file& this to allow chaining
     */
    template<typename T>
    sd_file &operator<<(T data)
    {
        return *this << std::to_string(data);
    }

private:
    static bool opened, initialized;
    File file_intern;
};

#endif
