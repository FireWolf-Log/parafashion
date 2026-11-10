#include "xerror_limiter.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))
#include <X11/Xlib.h>
#include <dlfcn.h>
#include <cstdio>
#include <chrono>

static bool s_enable_xerror_interception = false;
static bool s_is_ignoring_errors = false;
static XErrorHandler s_real_prev_handler = nullptr;

static int RateLimitedXErrorHandler(Display *display, XErrorEvent *err)
{
    static auto lastTime = std::chrono::steady_clock::time_point{};
    static int suppressedCount = 0;
    static int lastErrCode = 0;
    static int lastReqCode = 0;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();

    if (lastTime.time_since_epoch().count() == 0 ||
        err->error_code != lastErrCode ||
        err->request_code != lastReqCode ||
        elapsed >= 5) {
        if (suppressedCount > 0 && err->error_code == lastErrCode && err->request_code == lastReqCode) {
            fprintf(stderr, "Ignoring Xlib error: error code %d request code %d (repeated %d times)\n",
                    err->error_code, err->request_code, suppressedCount);
        } else {
            fprintf(stderr, "Ignoring Xlib error: error code %d request code %d\n",
                    err->error_code, err->request_code);
        }
        lastErrCode = err->error_code;
        lastReqCode = err->request_code;
        lastTime = now;
        suppressedCount = 0;
    } else {
        suppressedCount++;
    }
    return 0;
}

extern "C" XErrorHandler XSetErrorHandler(XErrorHandler handler)
{
    typedef XErrorHandler (*XSetErrorHandlerFunc)(XErrorHandler);
    static XSetErrorHandlerFunc real_XSetErrorHandler = nullptr;
    if (!real_XSetErrorHandler) {
        real_XSetErrorHandler = (XSetErrorHandlerFunc)dlsym(RTLD_NEXT, "XSetErrorHandler");
    }

    if (!real_XSetErrorHandler) {
        return nullptr;
    }

    if (!s_enable_xerror_interception) {
        return real_XSetErrorHandler(handler);
    }

    if (!s_is_ignoring_errors) {
        // AntTweakBar's IgnoreXErrors() is setting InactiveErrorHandler.
        // We install RateLimitedXErrorHandler instead so it doesn't flood stderr.
        s_is_ignoring_errors = true;
        s_real_prev_handler = real_XSetErrorHandler(RateLimitedXErrorHandler);
        return s_real_prev_handler;
    } else {
        // AntTweakBar's RestoreXErrors() is restoring s_PrevErrorHandler.
        s_is_ignoring_errors = false;
        XErrorHandler old = real_XSetErrorHandler(handler);
        s_real_prev_handler = nullptr;
        return old;
    }
}

void EnableXErrorLimiter()
{
    s_enable_xerror_interception = true;
}
#else
void EnableXErrorLimiter()
{
}
#endif
