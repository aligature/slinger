#pragma once

#include <string>

inline void open_browser(std::string const& auth_uri)
{
#if defined(_WIN32) && !defined(__cplusplus_winrt)
   // NOTE: Windows desktop only.
   auto r = ShellExecuteA(NULL, "open", conversions::utf16_to_utf8(auth_uri).c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
   // NOTE: OS X only.
   std::string browser_cmd("open \"" + auth_uri + "\"");
   system(browser_cmd.c_str());
#else
   // NOTE: Linux/X11 only.
   string_t browser_cmd("xdg-open \"" + auth_uri + "\"");
   system(browser_cmd.c_str());
#endif
}
