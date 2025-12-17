// ============================================================================
// Screen Compilation Unit
// ============================================================================
// Single compilation unit for all screen implementations.
// This ensures screen .cpp files are compiled with correct build flags
// while keeping them organized in the screens/ subdirectory.

#include "board_config.h"

#if HAS_DISPLAY

// Include display driver implementations
#include "drivers/tft_espi_driver.cpp"

// Include all screen implementations
#include "screens/splash_screen.cpp"
#include "screens/info_screen.cpp"
#include "screens/test_screen.cpp"

#endif // HAS_DISPLAY
