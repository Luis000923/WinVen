#ifndef CONFIG_GUI_H
#define CONFIG_GUI_H

#include "WindowManager.h"
#include <windows.h>

// Opens the configuration window in a separate thread.
// Ensures GDI+ is initialized if not already (though ideally main app does it).
void OpenConfigWindow(WindowManager *mgr, HINSTANCE hInst,
                      DWORD dwMainThreadId);

#endif // CONFIG_GUI_H
