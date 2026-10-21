//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


#if !defined (__PING_COMMAND_HPP__)
#define __PING_COMMAND_HPP__


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#include <string.h>  // For using the std::string class
#include <stdint.h>


bool ping(std::string url, uint32_t count);


#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // __PING_COMMAND_HPP__
