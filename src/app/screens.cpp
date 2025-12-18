// ============================================================================
// Screen Compilation Unit
// ============================================================================
// Single compilation unit for all screen implementations.
// This ensures screen .cpp files are compiled with correct build flags
// while keeping them organized in the screens/ subdirectory.
//
// Note: Display driver .cpp files are included in display_manager.cpp,
// not here, since the display manager is responsible for driver lifecycle.

#include "board_config.h"

#if HAS_DISPLAY

// Include all screen implementations
#include "screens/splash_screen.cpp"
#include "screens/info_screen.cpp"
#include "screens/test_screen.cpp"

#if HAS_IMAGE_API
#include "screens/direct_image_screen.cpp"
#endif

#endif // HAS_DISPLAY

// ============================================================================
// Touch Compilation Unit
// ============================================================================
#if HAS_TOUCH

// Include touch driver implementations (conditional based on selection)
#if TOUCH_DRIVER == TOUCH_DRIVER_XPT2046
#include "drivers/xpt2046_driver.cpp"
#elif TOUCH_DRIVER == TOUCH_DRIVER_FT6236
// Future: FT6236 capacitive touch driver
#endif

// Note: touch_manager.cpp is compiled separately by Arduino build system
// (just like display_manager.cpp)

#endif // HAS_TOUCH
