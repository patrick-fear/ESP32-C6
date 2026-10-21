//==============================================================================
//
//  File   :
//  Purpose:
//
//==============================================================================


// Enable touch screen, Waveshare ESP32-C6 doesn't have touch
//#define ENALBE_TOUCH_SCREEN     1


#include <stdio.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lvgl_port.h>
#ifdef ENALBE_TOUCH_SCREEN
    #include <esp_lcd_touch_tt21100.h>
#endif  // ENALBE_TOUCH_SCREEN
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <lvgl.h>
#include <sdkconfig.h>


// Custom includes
#include "main.hpp"
#include "TFT_backlight.hpp"
#include "TFT_display.hpp"


// module name for ESP_LOG
const static char *TAG = "tft";


// Use black screen instead of a colored screen
//#define FILL_SCREEN_BLACK 1


// Values to pass to lv_color_hex() since there is no equivalent for lv_palette_main()
#define COLOR_HEX_WHITE     0xFFFFFF
#define COLOR_HEX_BLACK     0x000000

// Could adjust 'SDK Configuraiton editor' to have a different default value for text label
#define TEXT_FONT           lv_font_montserrat_24

// Max length for text to write out.  For now we'll assume 5 lines of text, 4 newlines and 1 NULL
#define MAX_TEXT_LENGTH     ( (MAX_LINE_LENGTH * 5) + 4 + 1 )

// Default lv_obj_set_pos() values for text label
#define TEXT_X_POSITION     5   // text label x value for lv_obj_set_pos()
#define TEXT_Y_POSITION     5   // text label y value for lv_obj_set_pos()


/* LCD size */
#define EXAMPLE_LCD_H_RES               (172)   // LCD horitontal pixel size
#define EXAMPLE_LCD_V_RES               (320)   // LCD vertical pixel size

/* LCD settings */
#define EXAMPLE_LCD_SPI_NUM             (SPI2_HOST)
#define EXAMPLE_LCD_PIXEL_CLK_HZ        (40 * 1000 * 1000)
#define EXAMPLE_LCD_CMD_BITS            (8)
#define EXAMPLE_LCD_PARAM_BITS          (8)
#define EXAMPLE_LCD_BITS_PER_PIXEL      (16)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE    (1)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT    (50)
//#define EXAMPLE_LCD_BL_ON_LEVEL         (1)

/* LCD pins */
#define EXAMPLE_LCD_GPIO_SCLK           (7)
#define EXAMPLE_LCD_GPIO_MOSI           (6)
#define EXAMPLE_LCD_GPIO_RST            (21)
#define EXAMPLE_LCD_GPIO_DC             (15)
#define EXAMPLE_LCD_GPIO_CS             (14)
//#define EXAMPLE_LCD_GPIO_BL             (22)

/* LCD IO and panel */
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;
#ifdef ENALBE_TOUCH_SCREEN
    static esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    static esp_lcd_touch_handle_t touch_handle = NULL;
#endif  // ENALBE_TOUCH_SCREEN

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
#ifdef ENALBE_TOUCH_SCREEN
    static lv_indev_t *lvgl_touch_indev = NULL;
    static i2c_master_bus_handle_t i2c_handle = NULL;
#endif  // ENALBE_TOUCH_SCREEN



/*******************************************************************************
 * @brief           Initlaized the LCD and backlight
 *
 * @return          esp_err_t
 ******************************************************************************/
static esp_err_t app_lcd_init(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGD(TAG, "app_lcd_init");

    //* LCD backlight */
    //gpio_config_t bk_gpio_config = {
    //    .mode = GPIO_MODE_OUTPUT,
    //    .pin_bit_mask = 1ULL << EXAMPLE_LCD_GPIO_BL
    //};
    //ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    backlight_setup();

    /* LCD initialization */
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .mosi_io_num = EXAMPLE_LCD_GPIO_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = EXAMPLE_LCD_GPIO_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .data4_io_num = -1,  //new
        .data5_io_num = -1,  //new
        .data6_io_num = -1,  //new
        .data7_io_num = -1,  //new
        .data_io_default_level = false,  //new
        .max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t),
        .flags = 0,  //new
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,  //new
        .intr_flags = 0,  //new
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(EXAMPLE_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = EXAMPLE_LCD_GPIO_CS,
        .dc_gpio_num = EXAMPLE_LCD_GPIO_DC,
        .spi_mode = 0,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = 0,  //new
        .user_ctx = NULL,  //new
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .cs_ena_pretrans = 0,  //new
        .cs_ena_posttrans = 0,  //new
        .flags = {
            .dc_high_on_cmd = false,  //new
            .dc_low_on_data = false,  //new
            .dc_low_on_param = false,  //new
            .octal_mode = false,  //new
            .quad_mode = false,  //new
            .sio_mode = false,  //new
            .lsb_first = false,  //new
            .cs_high_active = false,  //new
        } //new
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_SPI_NUM, &io_config, &lcd_io), err, TAG, "New panel IO failed");

    ESP_LOGD(TAG, "Install LCD driver");
    {
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = EXAMPLE_LCD_GPIO_RST,
    #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
            .rgb_endian = LCD_RGB_ENDIAN_RGB,  // Change from BGR to RGB for ESP32-C6 Waveshare module
    #else
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
    #endif
            .data_endian = LCD_RGB_DATA_ENDIAN_BIG,  //new
            .bits_per_pixel = EXAMPLE_LCD_BITS_PER_PIXEL,
            .flags = 0,  //new
            .vendor_config = NULL,  //new
        };
        //ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_SPI_NUM, &io_config, &lcd_io), err, TAG, "New panel IO failed");
        ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(lcd_io, &panel_config, &lcd_panel), err, TAG, "New panel failed");
    }

    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(lcd_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));  // Added for ESP32-C6 Waveshare LCD panel, invert the colorings so BLUE is not YELLOW
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd_panel, 34, 0));  // Slight offset in the x direction on the LCD panel


    /* LCD backlight on */
    //ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_LCD_GPIO_BL, EXAMPLE_LCD_BL_ON_LEVEL));
    backlight_set_brightness(50);

    return ret;

err:
    if (lcd_panel) {
        esp_lcd_panel_del(lcd_panel);
    }
    if (lcd_io) {
        esp_lcd_panel_io_del(lcd_io);
    }
    spi_bus_free(EXAMPLE_LCD_SPI_NUM);
    return ret;
}


/*******************************************************************************
 * @brief           Initlaized the LVGL
 *
 * @return          esp_err_t
 ******************************************************************************/
static esp_err_t app_lvgl_init(void)
{
    ESP_LOGD(TAG, "app_lvgl_init");

    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,         /* LVGL task priority */
        .task_stack = 4096,         /* LVGL task stack size */
        .task_affinity = -1,        /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500,   /* Maximum sleep in LVGL task */
        .task_stack_caps = 0,  //new
        .timer_period_ms = 5        /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .control_handle = NULL,  //new
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = EXAMPLE_LCD_DRAW_BUFF_DOUBLE,
        .trans_size = 0,  //new
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,  //new
            .sw_rotate = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,
#endif
            .full_refresh = false,  //new
            .direct_mode = false,  //new
        }
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    lv_display_set_rotation(lvgl_disp, LV_DISPLAY_ROTATION_270); // need to add lvgl_port_display_cfg_t flag.sw_rotate


    /* Add touch input (for selected screen) */
    #ifdef ENALBE_TOUCH_SCREEN
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lvgl_disp,
            .handle = touch_handle,
        };
        lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    #endif  // ENALBE_TOUCH_SCREEN

    return ESP_OK;
}



/*******************************************************************************
 * @brief           Main function to initialized the TFT displahy
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t TFT_display_setup(void)
{
    ESP_LOGI(TAG, "setup");
    esp_log_level_set("tft", ESP_LOG_DEBUG);

    /* LCD HW initialization */
    ESP_ERROR_CHECK(app_lcd_init());

    /* Touch initialization */
    #ifdef ENALBE_TOUCH_SCREEN
        ESP_ERROR_CHECK(app_touch_init());
    #endif  // ENALBE_TOUCH_SCREEN

    /* LVGL initialization */
    ESP_ERROR_CHECK(app_lvgl_init());

    /* Examples to show LVGL objects */
    // Dispaly message
    int8_t status = -2;
    char string_line_1[MAX_LINE_LENGTH];
    char string_line_2[MAX_LINE_LENGTH];
    char string_line_3[MAX_LINE_LENGTH];
    char string_line_4[MAX_LINE_LENGTH];
    sprintf(string_line_1, "%s %s", MAIN_NAME_SHORT, MAIN_VERSION);
    sprintf(string_line_2, "Booting");
    string_line_3[0] = '\0';
    string_line_4[0] = '\0';
    TFT_display_message(status, string_line_1, string_line_2, string_line_3, string_line_4);

    // Log which fonts are aviable
    // To enable other fonts need to use 'idf.py menuconfig' (SKU Configuraiton editor)
    // and then check button enable a new font type/size.
    #ifdef CONFIG_LV_FONT_MONTSERRAT_14
        ESP_LOGD(TAG, "LV_FONT_MONTSERRAT_14");
    #endif  // CONFIG_LV_FONT_MONTSERRAT_14
    #ifdef CONFIG_LV_FONT_MONTSERRAT_20
        ESP_LOGD(TAG, "LV_FONT_MONTSERRAT_20");
    #endif  // CONFIG_LV_FONT_MONTSERRAT_20
    #ifdef CONFIG_LV_FONT_MONTSERRAT_24
        ESP_LOGD(TAG, "LV_FONT_MONTSERRAT_24");
    #endif  // CONFIG_LV_FONT_MONTSERRAT_24
    #ifdef CONFIG_LV_FONT_MONTSERRAT_30
        ESP_LOGD(TAG, "LV_FONT_MONTSERRAT_30");
    #endif  // CONFIG_LV_FONT_MONTSERRAT_30
    #ifdef CONFIG_LV_FONT_MONTSERRAT_34
        ESP_LOGD(TAG, "LV_FONT_MONTSERRAT_34");
    #endif  // CONFIG_LV_FONT_MONTSERRAT_34
    #ifdef CONFIG_LV_FONT_MONTSERRAT_40
        ESP_LOGD(TAG, "LV_FONT_MONTSERRAT_40");
    #endif  // CONFIG_LV_FONT_MONTSERRAT_40

    return ESP_OK;
};


/*******************************************************************************
 * @brief           Display a message on the TFT
 *
 * @param[in]       status check service status value
 * @param[in]       line_1 TFT dispaly value for line 1
 * @param[in]       line_2 TFT dispaly value for line 2
 * @param[in]       line_3 TFT dispaly value for line 3
 * @param[in]       line_4 TFT dispaly value for line 4
 *
 * @return          esp_err_t
 ******************************************************************************/
esp_err_t TFT_display_message(int8_t status, char* line_1, char* line_2, char* line_3, char* line_4)
{
    lv_obj_t *screen = lv_scr_act(); // Get the active screen
    lv_color_t color_bg;
    lv_color_t color_text;
    char text[MAX_TEXT_LENGTH];

    //ESP_LOGI(TAG, "message");
    ESP_LOGD(TAG, "message status=%d", status);
    ESP_LOGD(TAG, "message line_1=%s", line_1);
    ESP_LOGD(TAG, "message line_2=%s", line_2);
    ESP_LOGD(TAG, "message line_3=%s", line_3);
    ESP_LOGD(TAG, "message line_4=%s", line_4);

    switch (status) {
        case 1:
            #ifdef FILL_SCREEN_BLACK
                color_bg = lv_color_black();  // Could also use lv_color_hex(0x000000) or lv_palette_main(LV_PALETTE_RED)
                color_text = lv_palette_main(LV_PALETTE_LIGHT_GREEN);
            #else
                color_bg = lv_palette_main(LV_PALETTE_LIGHT_GREEN);
                color_text = lv_color_black();
            #endif //FILL_SCREEN_BLACK
            break;
        case 0:
            #ifdef FILL_SCREEN_BLACK
                color_bg = lv_color_black();
                color_text = lv_palette_main(LV_PALETTE_RED);
            #else
                color_bg = lv_palette_main(LV_PALETTE_RED);
                color_text = lv_color_black();
            #endif //FILL_SCREEN_BLACK
            break;
        case -1:
            #ifdef FILL_SCREEN_BLACK
                color_bg = lv_color_black();
                color_text = lv_palette_main(LV_PALETTE_YELLOW);
            #else
                color_bg = lv_palette_main(LV_PALETTE_YELLOW);
                color_text = lv_color_black();
            #endif //FILL_SCREEN_BLACK
            break;
        default:
            #ifdef FILL_SCREEN_BLACK
                color_bg = lv_color_black();
                color_text = lv_color_hex(COLOR_HEX_WHITE);
            #else
                color_bg = lv_color_hex(COLOR_HEX_WHITE);
                color_text = lv_color_black();
            #endif // FILL_SCREEN_BLACK
            break;
    }

    // Built up text message to dispaly
    sprintf(text, "%s\n%s\n%s\n%s", line_1, line_2, line_3, line_4);

    /* Task lock */
    lvgl_port_lock(0);

    // Clear screen
    lv_obj_clean(screen);

    // Set background
    lv_obj_set_style_bg_color(screen, color_bg, LV_PART_MAIN);

    /* Label */
    lv_obj_t *label = lv_label_create(screen);

    // Set x/y location
    lv_obj_set_pos(label, TEXT_X_POSITION, TEXT_Y_POSITION);

    // Set font and size
    //lv_obj_set_style_text_font(label, &TEXT_FONT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &TEXT_FONT, 0);

    // Set the label color
    lv_obj_set_style_text_color(label, color_text, 0);

    // Set the text value to display
    lv_label_set_text(label, text);

    /* Task unlock */
    lvgl_port_unlock();

    return ESP_OK;
}


#ifdef DEBUG_EXAMPLES_ONLY

/*******************************************************************************
 * @brief           Display a example graphics on the TFT
 *
 * @return          esp_err_t
 ******************************************************************************/
static esp_err_t app_main_display(void)
{
    lv_obj_t *screen = lv_scr_act();

    /* Task lock */
    lvgl_port_lock(0);

    // Clear screen
    lv_obj_clean(screen);

    /* Your LVGL objects code here .... */

    // Set background
    //lv_obj_set_style_bg_color(background, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    //lv_obj_set_style_bg_color(background, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);


    /* Create image */
    //lv_obj_t *img_logo = lv_img_create(screen);
    //lv_img_set_src(img_logo, &esp_logo);
    //lv_obj_align(img_logo, LV_ALIGN_TOP_MID, 0, 20);

    /* Label */
    lv_obj_t *label = lv_label_create(screen);
    lv_obj_set_width(label, EXAMPLE_LCD_H_RES);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);


    // Set font and size
    //lv_obj_set_style_text_font(label, &TEXT_FONT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &TEXT_FONT, 0);


    // Set the label color
    //lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_RED), 0);


#if LVGL_VERSION_MAJOR == 8
    lv_label_set_recolor(label, true);
    //lv_label_set_text(label, "#FF0000 "LV_SYMBOL_BELL" Hello world Espressif and LVGL "LV_SYMBOL_BELL"#\n#FF9400 "LV_SYMBOL_WARNING" For simplier initialization, use BSP "LV_SYMBOL_WARNING" #");
    lv_label_set_text(label, "Hello world\nline2\nline\nline4");
#else
    //lv_label_set_text(label, LV_SYMBOL_BELL" Hello world Espressif and LVGL "LV_SYMBOL_BELL"\n "LV_SYMBOL_WARNING" For simplier initialization, use BSP "LV_SYMBOL_WARNING);
    lv_label_set_text(label, "Hello world\nline_2\nline_3\nline_4");
#endif
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 20);

    /* Button */
    #ifdef ENALBE_TOUCH_SCREEN
        lv_obj_t *btn = lv_btn_create(scr);
        label = lv_label_create(btn);
        lv_label_set_text_static(label, "Rotate screen");
        lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
        lv_obj_add_event_cb(btn, _app_button_cb, LV_EVENT_CLICKED, NULL);
    #endif  // ENALBE_TOUCH_SCREEN


    /* Task unlock */
    lvgl_port_unlock();

    return ESP_OK;
}


/*******************************************************************************
 * @brief           Display rectangles on TFT
 * Example displaying rectables, useful for making sure have x/y, roation and offset correctly setup
 *
 * @return          esp_err_t
 ******************************************************************************/
static esp_err_t app_main_filled_rectangle(void)
{
    lv_obj_t *screen = lv_scr_act(); // Get the active screen

    /* Task lock */
    lvgl_port_lock(0);

    // Clear screen
    lv_obj_clean(screen);

    // Set background
    lv_obj_t* background = lv_scr_act();
    //lv_obj_set_style_bg_color(background, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(background, lv_color_black(), LV_PART_MAIN);
    //lv_obj_set_style_bg_color(background, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);

    lv_obj_t *my_rect = lv_obj_create(screen); // Create a new object on the screen

    lv_obj_set_size(my_rect, 100, 200); // Set width to 100 pixels, height to 50 pixels
    lv_obj_set_pos(my_rect, 25, 25);  // Set position to (20, 30)

    lv_obj_set_style_bg_color(my_rect, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN); // Set background color to blue
    lv_obj_set_style_border_width(my_rect, 2, LV_PART_MAIN); // Set border width
    lv_obj_set_style_border_color(my_rect, lv_color_make(0xFF, 0x00, 0x00), LV_PART_MAIN); // Set border color to red
    lv_obj_set_style_radius(my_rect, 10, LV_PART_MAIN); // Set rounded corners with a radius of 10

    /* Task unlock */
    lvgl_port_unlock();

    return ESP_OK;
}
#endif  // DEBUG_EXAMPLES_ONLY
