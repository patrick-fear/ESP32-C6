//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#if !defined (__RBG_LED_HPP__)
#define __RBG_LED_HPP__


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#include <stdint.h>
#include <esp_err.h>


esp_err_t rgb_led_setup(void);
esp_err_t rgb_led_Color(uint32_t red, uint32_t green, uint32_t blue);
esp_err_t rgb_led_Hue(uint16_t hue);
esp_err_t rgb_led_cycle_Color(void);
esp_err_t rgb_led_cycle_Hue(void);

#ifdef __cplusplus
}
#endif  // __cplusplus


#endif  // __RBG_LED_HPP__

