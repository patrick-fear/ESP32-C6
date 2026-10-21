//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#include <stdio.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/ledc.h>
#include <sdkconfig.h>


// Custom includes
#include "TFT_backlight.hpp"


// module name for ESP_LOG
const static char *TAG = "bl";


// Backlight pin configuration
#define LCD_BL_PIN              22 // Replace X with your actual backlight pin
#define LCD_BL_PWM_CHANNEL      LEDC_CHANNEL_0
#define LCD_BL_PWM_TIMER        LEDC_TIMER_0
#define LCD_BL_PWM_FREQ_HZ      5000 // 5kHz frequency
//#define LCD_BL_PWM_FREQ_HZ      44100  // 44.1kHz frequency
#define LCD_BL_PWM_RESOLUTION   LEDC_TIMER_8_BIT // 8-bit resolution (0-255)


/*******************************************************************************
 * @brief           Function to initialize LEDC for backlight control
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t backlight_setup(void) {
    ESP_LOGI(TAG, "bl_init");

    ledc_timer_config_t ledc_timer = {
        .speed_mode         = LEDC_LOW_SPEED_MODE,
        .duty_resolution    = LCD_BL_PWM_RESOLUTION,
        .timer_num          = LCD_BL_PWM_TIMER,
        .freq_hz            = LCD_BL_PWM_FREQ_HZ,
        .clk_cfg            = LEDC_AUTO_CLK,
        .deconfigure        = false,  //new
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num           = LCD_BL_PIN,
        .speed_mode         = LEDC_LOW_SPEED_MODE,
        .channel            = LCD_BL_PWM_CHANNEL,
        .intr_type          = LEDC_INTR_DISABLE,
        .timer_sel          = LCD_BL_PWM_TIMER,
        .duty               = 0, // Initial duty cycle (off)
        .hpoint             = 0,
        .sleep_mode         = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,  //new
        .flags = {
            .output_invert  = 0,  //new
        },
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // Start PWM
    ESP_ERROR_CHECK(ledc_fade_func_install(0));

    return ESP_OK;
}


/*******************************************************************************
 * @brief           Function to set backlight brightness
 *
 * @param[in]       brightness brightness level for backlight
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t backlight_set_brightness(uint8_t brightness) {
    ESP_LOGD(TAG, "bl set %d", brightness);

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_BL_PWM_CHANNEL, brightness));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_BL_PWM_CHANNEL));

    return ESP_OK;
}
