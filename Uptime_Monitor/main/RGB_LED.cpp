
//==============================================================================
//
//  File   :
//  Purpose:  led_strip component documenation
//            https://github.com/espressif/idf-extra-components/tree/master/led_strip
//
//==============================================================================


#include <stdio.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <led_strip.h>
#include <sdkconfig.h>


// Custom includes
#include "RGB_LED.hpp"


// module name for ESP_LOG
static const char *TAG = "RGB";


// GPIO that is connected to the RGB
#define BLINK_GPIO          8

// Set to 1 to use DMA for driving the LED strip, 0 otherwise
// Please note the RMT DMA feature is only available on chips e.g. ESP32-S3/P4
#define LED_STRIP_USE_DMA   0

#if LED_STRIP_USE_DMA
    // Numbers of the LED in the strip
    #define LED_STRIP_LED_COUNT             1
    #define LED_STRIP_MEMORY_BLOCK_WORDS    1024 // this determines the DMA block size
#else
    // Numbers of the LED in the strip
    #define LED_STRIP_LED_COUNT             1
    #define LED_STRIP_MEMORY_BLOCK_WORDS    0 // let the driver choose a proper memory block size automatically
#endif // LED_STRIP_USE_DMA

// GPIO assignment
#define LED_STRIP_GPIO_PIN  8

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)


/// Create the LED strip object
static led_strip_handle_t led_strip = NULL;

// Flag to enalbe semaphore usuage for RGB access
#define USE_SEMAPHORE 1

#ifdef USE_SEMAPHORE
    // Declare a handle for the binary semaphore
    SemaphoreHandle_t xBinarySemaphore_RGB;
#endif // USE_SEMAPHORE




/*******************************************************************************
 * @brief           Setup the RGB LED handles and init RGB
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t rgb_led_setup(void)
{
    ESP_LOGD(TAG, "rgb_led_setup");

    #ifdef USE_SEMAPHORE
        // Create the binary semaphore.
        // The semaphore is initially "empty" (taken), meaning the receiver task will block immediately.
        xBinarySemaphore_RGB = xSemaphoreCreateBinary();
    #endif  // USE_SEMAPHORE

    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN, // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_COUNT,      // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,        // LED strip model
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB, // The color order of the strip: RGB
        .flags = {
            .invert_out = false, // don't invert the output signal
        }
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .mem_block_symbols = LED_STRIP_MEMORY_BLOCK_WORDS, // the memory block size used by the RMT channel
        .flags = {
            .with_dma = LED_STRIP_USE_DMA,     // Using DMA can improve performance when driving more LEDs
        }
    };


    // LED Strip object handle
    //led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");

    // Set all LED off to clear all pixels
    ESP_ERROR_CHECK(led_strip_clear(led_strip));

    #ifdef USE_SEMAPHORE
        // Give the semaphore to notify the receiver task
        configASSERT(xSemaphoreGive(xBinarySemaphore_RGB) == pdTRUE);
    #endif  // USE_SEMAPHORE

    //return led_strip;
    return ESP_OK;
}


/*******************************************************************************
 * @brief           Set RGB LED to given color
 *
 * @param[in]       red level from 0 to 255
 * @param[in]       green level from 0 to 255
 * @param[in]       blue level from 0 to 255
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t rgb_led_Color(uint32_t red, uint32_t green, uint32_t blue) {
    ESP_LOGD(TAG, "r=%3d g=%3d b=%3d", red, green, blue);

    #ifdef USE_SEMAPHORE
        // Attempt to take the semaphore, blocking indefinitely until it's available
        configASSERT(xSemaphoreTake(xBinarySemaphore_RGB, portMAX_DELAY) == pdTRUE);
    #endif  // USE_SEMAPHORE

    //Set all LED off to clear all pixels
    led_strip_clear(led_strip);

    // Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color
    led_strip_set_pixel(led_strip, 0, red, green, blue);

    // Refresh the strip to send data
    led_strip_refresh(led_strip);

    #ifdef USE_SEMAPHORE
        // Give the semaphore to notify the receiver task
        configASSERT(xSemaphoreGive(xBinarySemaphore_RGB) == pdTRUE);
    #endif  // USE_SEMAPHORE

    return ESP_OK;
}


/*******************************************************************************
 * @brief           Set RGB LED to given hue
 *
 * @param[in]       hue level from 0 to 360
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t rgb_led_Hue(uint16_t hue) {

    //leds[0] = CHSV(gHue, 255, 255);  // Set LED to current hue, full saturation, full brightness
    //leds[0] = CHSV(gHue, 255, 128);  // Set LED to current hue, full saturation, 1/2 brightness
    uint8_t saturation = 255;
    uint8_t value = 255;
    ESP_LOGD(TAG, "h=%3d s=%3d v=%3d", hue, saturation, value);

    #ifdef USE_SEMAPHORE
        // Attempt to take the semaphore, blocking indefinitely until it's available
        configASSERT(xSemaphoreTake(xBinarySemaphore_RGB, portMAX_DELAY) == pdTRUE);
    #endif  // USE_SEMAPHORE

    //Set all LED off to clear all pixels
    //led_strip_clear(led_strip);

    led_strip_set_pixel_hsv(led_strip, 0, hue, saturation, value);

    // Refresh the strip to send data
    led_strip_refresh(led_strip);

    #ifdef USE_SEMAPHORE
        // Give the semaphore to notify the receiver task
        configASSERT(xSemaphoreGive(xBinarySemaphore_RGB) == pdTRUE);
    #endif  // USE_SEMAPHORE

    return ESP_OK;
}


/*******************************************************************************
 * @brief           Cycle RGB LED to next color in rainbow
 *
 * The red/green/blue levels cycle in the rainbow from Red to Violet and the start back over at Red
 * https://www.rapidtables.com/web/color/RGB_Color.html
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t rgb_led_cycle_Color(void) {
    static uint8_t ii = 0;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint8_t rr;

    rr =  ii % 7;
    switch (rr) {
        case 0:
            //color = CRGB::Red;
            red = 255;
            green = 0;
            blue = 0;
            break;
        case 1:
            //color = CRGB::Orange;
            red = 255;
            green = 165;
            blue = 0;
            break;
        case 2:
            //color = CRGB::Yellow;
            red = 255;
            green = 255;
            blue = 0;
            break;
        case 3:
            //color = CRGB::Green;
            red = 0;
            green = 255;
            blue = 0;
            break;
        case 4:
            //color = CRGB::Blue;
            red = 0;
            green = 0;
            blue = 255;
            break;
        case 5:
            //color = CRGB::Indigo;
            red = 75;
            green = 0;
            blue = 130;
            break;
        case 6:
            //color = CRGB::Violet;
            red = 238;
            green = 130;
            blue = 238;
            break;
        default:
            //color = CRGB::Black;
            red = 0;
            green = 0;
            blue = 0;
            break;
    }

    rgb_led_Color(red, green, blue);
    ii++;

    return ESP_OK;
}

/*******************************************************************************
 * @brief           Cycle RGB LED to next hue level
 *
 * The hue level cycle from 0 to 360 and then back to 0.
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t rgb_led_cycle_Hue(void) {
    static uint16_t hue = 0;
    rgb_led_Hue(hue);
    hue++; // Increment hue

    // wraps from 0 to 360
    if (hue > 360) {
        //ESP_LOGI(TAG, "hue reset");
        hue = 0;
    }

    return ESP_OK;
}




