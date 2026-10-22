This is a fairly simple ESP32-C project to monitor some website like a poor man's Uptime Kuma.

The ESP32 will boot up, get an Wi-FI IP address, and then start do the checks.
The WiFi SSID/password are hard coded in the source, so need to recompile and flash if they need to change.
The ESP32 has a website you can navitate to but is unsecure.  Allos you to see the status and add/remove services.
The LCD display will also show basic info like SSID, IP Address, MAC Addr, and then green/red for basic status for pass/fail.
The RGG LED is just cyclying constantly as a way to show that the ESP32 is running.  If the ESP32 were to hang (stop running), then the RBG would stop cycling.

Comparsion of the ESP32 versions
    https://products.espressif.com/static/Espressif%20SoC%20Product%20Portfolio.pdf


# Waveshare ESP32-C6 1.47 inch LED
Main website
    https://www.waveshare.com/esp32-c6-lcd-1.47.htm

Wiki page and documentation
    https://www.waveshare.com/wiki/ESP32-C6-LCD-1.47

Schematic PDF:
    https://files.waveshare.com/wiki/ESP32-C6-LCD-1.47/ESP32-C6-LCD-1.47_schemetics.pdf


# ESP32-C6
ESP-IDF support ESP32-C6 nativelty, including the LP core version.


# Partition
The Waveshare ESP32-C6 board docs indicate a 4MB SPI flash.  The SPI flash is
actualy embedded inside the ESP32-C6 devcie.  But using the ESP-IDF tools, they
report back a 8MB SPI flash.  So Override the 'SDK Configuraiotn Editor' setting
'Flash Size' ESPTOOLPY_FLASHSIZE to set to 8MB.  Create custom partition file
'my_partitions_singleapp.csv' and indicate a 'app' of '4MB' to give half of
flash to the main program.  Change the 'SDK Configuraiton Editor' setting
'Partition Table' PARTITION_TALBE to 'Custom partition table CSV' and then
'Custom partition CSV file' ESPTOOLPY_FLASHSIZE to point to this partion file.


# Source files
New source file need to add to the CMakeList.txt file specificaly after adding
the file.  Or change CMakeList.txt file to add all *.* files:
    https://esp32.com/viewtopic.php?t=39259
Also need to add the header files to also include.


# Errors and Panic handler
Fatal errors and panic hanlder.  Adjust to reboot or not.  Use 'SDK
Configuraiton Editor' and 'Panic handler behavior' ESP_SYSTEM_PANIC to adjust
setting.  More info at:
    https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/api-guides/fatal-errors.html


# UART
The ESP32-C6 has a UART output over the USB port.  ESP-IDF default supports UART output.


# Logging
ESP-IDF uses ESP_LOGx() for logging.
ESP-IDF uses printf() for output to UART.
Mabye alternativaly can look at adding '#define CORE_DEBUG_LEVEL 4' at the top of the
source file before the '#include "esp_log.h"'.


# FreeRTOS
ESP-IDF has FreeROTS already builtin with multi tasking.  There is no 'setup()'
just the 'app_maiin()'.


# LCD Display
Use 'SDK Configuraiton Editor' setting 'Enable built-in fonts' and the differnet
check buttons.  Default is LV_FONT_MONTSERRAT_14, but can enable/disable more
fonts as needed.


# Uptime Monitor
The Uptime Monitor parts are based off the XDA article:
    https://www.xda-developers.com/built-uptime-monitor-esp32-powerful/
This artical then points to the github source:
    https://github.com/Incipiens/ESP32-Uptime-Monitoring/tree/main
Original source only handles HTTP GET responses of 200.  But there are other
respondes that are also valid like temporary move.  So add those to the valid
status as well.


# TODO
* Curretnly WiFi uses event group to notify http server and UptimeMonitor.  Could look more at doing a task suspend/notify, but need to consider what if ping is running when suspend?


# History
## v0.5.0 2025/11/22
* Add WiFi setup taskf.

## v0.6.0 2025/11/22
* Change task so that WiFi setup task sends signal to UptimeMonitor task to
  start.

## v0.7.0 2025/11/22
* Add TFT display task.  Use queue to send data to display task to then update the display.
* Add initial yellow color to display on first initialize, so that can tell that its not red failed yet.

## v0.8.0 2025/11/23
* Cleanup some Serial.print to instead use Serial.printf.
* Cleanup some logging to add messages for queue fails.
* Convert some Serial.print() to instead use ESP_LOGx().

## v0.9.0 2025/11/23
* Change platform tool version from '54.03.20' to '55.03.34'.  This will cause an increase in the image size.
* Change the partition from 'board_build.partitions = my_partitions.csv' to
  'board_build.partitions = my_partitions_8MB'.  ESP.getFlashChipSize() keeps
  reporting 8MB Flash.  The latest esptool https://github.com/espressif/esptool
  also shows as 8MB Flash and chip type as 'ESP32-C6FH8' and not 'ESP32-C6FH4'.
* Change the board from 'board = esp32-c6-devkitm-1' to 'board = esp32-c6-devkitc-1' to reflect 8MB Flash and Bluetooth.
* Add attiaional build flag 'build_flags = -D ESP32_C6_env'.

## v1.0.0 12/06/2025
* Inital version of conversion from PlatformIO to ESP-IDF.
* Set Panic from panic and reboot to panic only, so that can debug easier.
* Add FreeRTOS task to log out task details for debug.
* Change TFT from LovyanGFX to LVGL.
* Add HTTP commands.
* Add Ping commands.
* Add max string check for the getString() on HTTP to avoid overflow.
* Add mDNS support for WiFi interface.
* Add placeholder support in http server for Favicon URL, but disabled for now.

## v1.1.0 12/07/2025
* Add semaphore lock/unlock to RGB_LED to avoid any accidental calling of functions outside of Task_RGB_LED.

## v1.2.0 12/07/2025
* Additional columns/info added to the FreeRTOS debug logging.

## v1.3.0 12/08/2025
* Change Task_WiFi and Task_UptimeMonitor from using semaphore to instead use
  event group from wifi_setup.cpp and the event handler connect/disconnect.
  Task_WiFiSetup no longer notifies Task_UptimeMonitor.  Instead
  Task_UptimeMonitor uses the event group to enable/disable.  And then new
  Task_Server also uses the event group to start/stop the http server.
* Cleanup and delete unused function UptimeMonitor_setup.

## v1.4.0 01/02/2025
* Reduce the http buffer size MAX_USER_DATA_BUFFER_SIZE from x24 to x20.
