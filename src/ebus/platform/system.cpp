
#include "platform/system.hpp"

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif defined(POSIX)
#include <chrono>
#include <thread>
#endif

void ebus::sleepMs(uint32_t ms) {
#if defined(ESP32)
  vTaskDelay(pdMS_TO_TICKS(ms));
#elif defined(POSIX)
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}
