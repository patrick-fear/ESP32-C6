//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#if !defined (__MAIN_HPP__)
#define __MAIN_HPP__


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#include <stdint.h>
#include <freertos/queue.h>


// TFT displayl queue definntion
#define MAX_LINE_LENGTH 32
typedef struct {
    int8_t status;
    char string_line_1[MAX_LINE_LENGTH];
    char string_line_2[MAX_LINE_LENGTH];
    char string_line_3[MAX_LINE_LENGTH];
    char string_line_4[MAX_LINE_LENGTH];
} TFT_Queue_Data;

extern QueueHandle_t TFT_Message_Queue;


// Main app name and version
extern const char* MAIN_NAME_LONG;
extern const char* MAIN_NAME_SHORT;
extern const char* MAIN_VERSION;


void app_main(void);



#ifdef __cplusplus
}
#endif  // __cplusplus


#endif  // __MAIN_HPP__
