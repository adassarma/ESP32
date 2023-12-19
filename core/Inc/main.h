#pragma once

#include "freertos/FREERTOS.h"
#include "freertos/task.h"
#define pdSECOND pdMS_TO_TICKS(1000)

class Main final
{
    public:
    esp_err_t setup(void);
    void loop(void);
};