//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#if !defined (__TFT_DISPLAY_HPP__)
#define __TFT_DISPLAY_HPP__


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#include <stdint.h>
#include <esp_err.h>


esp_err_t TFT_display_setup(void);
esp_err_t TFT_display_message(int8_t status, char* line_1, char* line_2, char* line_3, char* line_4);


#ifdef __cplusplus
}
#endif  // __cplusplus


#endif  // __TFT_DISPLAY_HPP__
