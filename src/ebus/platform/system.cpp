
#include "platform/system.hpp"

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if __has_include(<esp_rom_sys.h>)
#include <esp_rom_sys.h>
#else
#include <rom/ets_sys.h>
#endif
#elif defined(POSIX)
#include <chrono>
#include <thread>
#endif

namespace ebus::detail {

void sleepMilli(uint32_t ms) {
#if defined(ESP32)
  vTaskDelay(pdMS_TO_TICKS(ms));
#elif defined(POSIX)
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

void sleepMicro(uint32_t us) {
#if defined(ESP32)
  esp_rom_delay_us(us);
#elif defined(POSIX)
  std::this_thread::sleep_for(std::chrono::microseconds(us));
#endif
}

}  // namespace ebus::detail