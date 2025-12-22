/**
 * @file lv_conf.h
 * Configuration file for LVGL v8.4
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// Allow LVGL config to react to board-specific overrides.
// build.sh defines BOARD_HAS_OVERRIDE and adds the override include path.
#ifdef BOARD_HAS_OVERRIDE
#include "board_overrides.h"
#endif

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 1 (1 byte per pixel), 8 (RGB332), 16 (RGB565), 32 (ARGB8888) */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color. Useful if the display has a 8 bit interface (e.g. SPI)*/
#define LV_COLOR_16_SWAP 0

/* Enable features to draw on transparent background */
#define LV_COLOR_SCREEN_TRANSP 0

/* Adjust color mix functions rounding. GPUs might calculate color mix differently */
#define LV_COLOR_MIX_ROUND_OFS 0

/*=========================
   MEMORY SETTINGS
 *=========================*/

/* 1: use custom malloc/free, 0: use the built-in `lv_mem_alloc()` and `lv_mem_free()` */
#define LV_MEM_CUSTOM 0
#if LV_MEM_CUSTOM == 0
  /* Size of the memory available for `lv_mem_alloc()` in bytes (>= 2kB)*/
  #define LV_MEM_SIZE (48U * 1024U)          /*[bytes]*/

  /* Set an address for the memory pool instead of allocating it as a normal array. */
  #define LV_MEM_ADR 0     /*0: unused*/
#else       /*LV_MEM_CUSTOM*/
  #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>   /*Header for the dynamic memory function*/
  #define LV_MEM_CUSTOM_ALLOC   malloc
  #define LV_MEM_CUSTOM_FREE    free
  #define LV_MEM_CUSTOM_REALLOC realloc
#endif     /*LV_MEM_CUSTOM*/

/* Number of the intermediate memory buffer used during rendering */
#define LV_DISP_DEF_REFR_PERIOD 30      /*[ms]*/

/* Input device read period in milliseconds */
#define LV_INDEV_DEF_READ_PERIOD 30     /*[ms]*/

/*=================
   HAL SETTINGS
 *=================*/

/* Default display refresh period. LVG will redraw changed areas with this period time*/
#define LV_DISP_DEF_REFR_PERIOD 30      /*[ms]*/

/* Use a custom tick source that tells the elapsed time in milliseconds */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
  #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"         /*Header for the system time function*/
  #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())    /*Expression evaluating to current system time in ms*/
#endif   /*LV_TICK_CUSTOM*/

/*================
 * FONT USAGE
 *===============*/

/* Montserrat fonts with various styles */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* Demonstrate special features */
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0  /*bpp = 3*/
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0  /*Hebrew, Arabic, Persian letters and all their forms*/
#define LV_FONT_SIMSUN_16_CJK            0  /*1000 most common CJK radicals*/

/*Pixel perfect monospace fonts*/
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

/* Optionally declare custom fonts here */
#define LV_FONT_CUSTOM_DECLARE

/* Set a default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*=================
 *  TEXT SETTINGS
 *=================*/

/* Select a character encoding for strings */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Break (wrap) long lines at space characters */
#define LV_TXT_BREAK_CHARS " ,.;:-_"

/* Support bidirectional texts */
#define LV_USE_BIDI 0

/* Support Arabic/Persian processing */
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*===================
 *  WIDGET USAGE
 *==================*/

/* Documentation of the widgets: https://docs.lvgl.io/latest/en/html/widgets/index.html */

#define LV_USE_ARC        1
#define LV_USE_BAR        0
#define LV_USE_BTN        0
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        0
#define LV_USE_LABEL      1
#define LV_USE_LINE       0
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SWITCH     0
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0

/* Used by SplashScreen */
#define LV_USE_SPINNER    1

/* Disable LVGL extra widgets we don't use (prevents dependency pulls).
 * These default to enabled in LVGL unless explicitly disabled. */
#define LV_USE_ANIMIMG                   0
#define LV_USE_CALENDAR                  0
#define LV_USE_CALENDAR_HEADER_ARROW     0
#define LV_USE_CALENDAR_HEADER_DROPDOWN  0
#define LV_USE_CHART                     0
#define LV_USE_COLORWHEEL                0
#define LV_USE_IMGBTN                    0
#define LV_USE_KEYBOARD                  0
#define LV_USE_LED                       0
#define LV_USE_LIST                      0
#define LV_USE_METER                     0
#define LV_USE_MENU                      0
#define LV_USE_MSGBOX                    0
#define LV_USE_SPINBOX                   0
#define LV_USE_TABVIEW                   0
#define LV_USE_TILEVIEW                  0
#define LV_USE_WIN                       0

/*==================
 * THEMES
 *==================*/

/* A simple, impressive and very complete theme */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
  /* 0: Light mode; 1: Dark mode */
  #define LV_THEME_DEFAULT_DARK 1

  /* 1: Enable grow on press */
  #define LV_THEME_DEFAULT_GROW 1

  /* Default transition time in [ms] */
  #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

/* A very simple theme that is a good starting point for a custom theme */
#define LV_USE_THEME_BASIC 0

/* A theme designed for monochrome displays */
#define LV_USE_THEME_MONO 0

/*==================
 * LAYOUTS
 *==================*/

/* A layout similar to Flexbox in CSS */
#define LV_USE_FLEX 0

/* A layout similar to Grid in CSS */
#define LV_USE_GRID 0

/*==================
 * OTHERS
 *==================*/

/* Support rendering animations */
#define LV_USE_ANIMATION 1

/* Use `switch` and `case` in API functions */
#define LV_USE_API_EXTENSION_V6 0
#define LV_USE_API_EXTENSION_V7 1

/* Logging */
#define LV_USE_LOG 0

/*-------------
 * Performance monitor
 *-----------*/
/* 1: Show CPU usage and FPS count in the right bottom corner
 * Note: We use a custom FPS overlay controlled via config instead */
#define LV_USE_PERF_MONITOR 0

#if LV_USE_PERF_MONITOR
  #ifndef LV_USE_PERF_MONITOR_POS
    #define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT
  #endif
#endif

/* Enable asserts */
#define LV_USE_ASSERT_NULL          1   /*Check if the parameter is NULL*/
#define LV_USE_ASSERT_MALLOC        1   /*Checks is the memory is successfully allocated*/
#define LV_USE_ASSERT_STYLE         0   /*Check style consistency*/
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /*Check memory integrity*/
#define LV_USE_ASSERT_OBJ           0   /*Check object integrity*/

/* Add a `user_data` to drivers and objects */
#define LV_USE_USER_DATA 1

/*==================
 *  COMPILER SETTINGS
 *==================*/

/* For big endian systems set to 1 */
#define LV_BIG_ENDIAN_SYSTEM 0

/* Define a custom attribute to `lv_tick_inc` function */
#define LV_ATTRIBUTE_TICK_INC

/* Define a custom attribute to `lv_timer_handler` function */
#define LV_ATTRIBUTE_TIMER_HANDLER

/* Define a custom attribute to `lv_disp_flush_ready` function */
#define LV_ATTRIBUTE_FLUSH_READY

/* Required alignment size for buffers */
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 1

/* Will be added where memories needs to be aligned (with -Os data might not be aligned to boundary by default) */
#define LV_ATTRIBUTE_MEM_ALIGN

/* Attribute to mark large constant arrays for example font's bitmaps */
#define LV_ATTRIBUTE_LARGE_CONST

/* Prefix performance critical functions to place them into a faster memory (e.g RAM) */
#define LV_ATTRIBUTE_FAST_MEM

/* Prefix variables that are used in GPU accelerated operations, often these need to be placed in RAM sections that are DMA accessible */
#define LV_ATTRIBUTE_DMA

/* Export integer constant to binding. This macro is used with constants in the form of LV_<CONST> */
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/* Extend the default -32k..32k coordinate range to -4M..4M by using int32_t for coordinates instead of int16_t */
#define LV_USE_LARGE_COORD 0

/*==================
 *   OTHERS
 *==================*/

/* Select a printf function to use */
#define LV_SPRINTF_CUSTOM 0

/* Test NULL parameters in API functions */
#define LV_USE_ASSERT_NULL 1

#endif /*LV_CONF_H*/
