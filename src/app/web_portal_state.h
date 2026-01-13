#ifndef WEB_PORTAL_STATE_H
#define WEB_PORTAL_STATE_H

struct DeviceConfig;

// Internal web portal state accessors used by helper modules.
// Implemented in web_portal.cpp.
bool web_portal_is_ap_mode_active();
DeviceConfig* web_portal_get_current_config();

#endif // WEB_PORTAL_STATE_H
