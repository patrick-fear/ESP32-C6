//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#if !defined (__TFT_BACKLIGHT_HPP__)
#define __TFT_BACKLIGHT_HPP__


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#include <stdint.h>
#include <esp_err.h>


esp_err_t backlight_setup(void);
esp_err_t backlight_set_brightness(uint8_t brightness);


#ifdef __cplusplus
}
#endif  // __cplusplus


#endif  // __TFT_BACKLIGHT_HPP__
