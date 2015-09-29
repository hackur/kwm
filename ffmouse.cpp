#include "ffmouse.h"

static const std::string OSASCRIPT_START = "osascript -e 'tell application \"System Events\" to tell process \"";
static const std::string OSASCRIPT_MID = "\" to perform action \"AXRaise\" of (first window whose name contains \"";
static const std::string OSASCRIPT_END = "\")'";

static pid_t pid;
static ProcessSerialNumber psn;
static std::vector<app_info> window_lst;
static int window_lst_focus_index;
static std::string window_lst_focus_name = "<no focus>";;

void detect_window_below_cursor()
{
    window_lst.clear();

    CFArrayRef osx_window_list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if(osx_window_list)
    {
        CFIndex osx_window_count = CFArrayGetCount(osx_window_list);
        for(CFIndex i = 0; i < osx_window_count; ++i)
        {
            CFDictionaryRef elem = (CFDictionaryRef)CFArrayGetValueAtIndex(osx_window_list, i);
            window_lst.push_back(app_info());
            CFDictionaryApplyFunction(elem, get_window_info, NULL);
        }

        CGEventRef event = CGEventCreate(NULL);
        CGPoint cursor = CGEventGetLocation(event);
        CFRelease(event);
        
        std::cout << "Mouse Pos: " << cursor.x << ", " << cursor.y << std::endl;
        for(int i = 0; i < window_lst.size(); ++i)
        {
            if(window_lst[i].layer == 0)
            {
                if(cursor.x >= window_lst[i].x && cursor.x <= window_lst[i].x + window_lst[i].width
                        && cursor.y >= window_lst[i].y && cursor.y <= window_lst[i].y + window_lst[i].height)
                {
                    if(window_lst_focus_name != window_lst[i].name || pid != window_lst[i].pid)
                    {
                        window_lst_focus_name = window_lst[i].name;
                        window_lst_focus_index = i;

                        // Strip single quotes and remaining part if found,
                        // because  it breaks applescript syntax
                        std::string win_title = window_lst[window_lst_focus_index].name;
                        std::size_t pos = win_title.find("'");
                        if(pos != std::string::npos)
                            win_title.erase(pos);

                        std::string applescript_cmd = OSASCRIPT_START +
                            window_lst[window_lst_focus_index].owner + OSASCRIPT_MID +
                            win_title + OSASCRIPT_END;
                        system(applescript_cmd.c_str());

                        if(pid != window_lst[i].pid)
                        {
                            pid = window_lst[i].pid;
                            GetProcessForPID(pid, &psn);
                        }
                        SetFrontProcessWithOptions(&psn, kSetFrontProcessFrontWindowOnly);
                    }
                    break;
                }

                /*
                std::cout << "Owner: " << window_lst[i].owner << std::endl;
                std::cout << "Name: " << window_lst[i].name << std::endl;
                std::cout << "PID: " << window_lst[i].pid << std::endl;
                std::cout << "Layer: " << window_lst[i].layer << std::endl;
                std::cout << "X: " << window_lst[i].x << std::endl;
                std::cout << "Y: " << window_lst[i].y << std::endl;
                std::cout << "Width: " << window_lst[i].width << std::endl;
                std::cout << "Height: " << window_lst[i].height << std::endl;
                */
            }
        }
        std::cout << "Keyboard focus: " << pid << std::endl;
    }
}

CGEventRef cgevent_callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon)
{
    if(type != kCGEventKeyDown && type != kCGEventKeyUp && type != kCGEventMouseMoved)
        return event;

    if(type == kCGEventKeyDown)
    {
        CGEventFlags flags = CGEventGetFlags(event);
        bool command_key = (flags & kCGEventFlagMaskCommand) == kCGEventFlagMaskCommand;
        bool alt_key = (flags & kCGEventFlagMaskAlternate) == kCGEventFlagMaskAlternate;
        bool ctrl_key = (flags & kCGEventFlagMaskControl) == kCGEventFlagMaskControl;
        if(command_key && alt_key && ctrl_key)
        {
            // Check for specific combinations to perform window tiling
            /*
            CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
            if(keycode == (CGKeyCode)0)
            {
            }
            */
            return event;
        }

        CGEventSetIntegerValueField(event, kCGKeyboardEventAutorepeat, 0);
        CGEventPostToPSN(&psn, event);
        return NULL;
    }
    else if(type == kCGEventKeyUp)
        return NULL;
    else if(type == kCGEventMouseMoved)
        detect_window_below_cursor();

    return event;
}

void get_window_info(const void *key, const void *value, void *context)
{
    CFStringRef k = (CFStringRef)key;
    std::string key_str = CFStringGetCStringPtr(k, kCFStringEncodingMacRoman);

    CFTypeID id = CFGetTypeID(value);
    if(id == CFStringGetTypeID())
    {
        CFStringRef v = (CFStringRef)value;
        if(CFStringGetCStringPtr(v, kCFStringEncodingMacRoman))
        {
            std::string value_str = CFStringGetCStringPtr(v, kCFStringEncodingMacRoman);
            if(key_str == "kCGWindowName")
                window_lst[window_lst.size()-1].name = value_str;
            else if(key_str == "kCGWindowOwnerName")
                window_lst[window_lst.size()-1].owner = value_str;
        }
    }
    else if(id == CFNumberGetTypeID())
    {
        int myint;
        CFNumberRef v = (CFNumberRef)value;
        CFNumberGetValue(v, kCFNumberSInt64Type, &myint);
        if(key_str == "kCGWindowLayer")
            window_lst[window_lst.size()-1].layer = myint;
        else if(key_str == "kCGWindowOwnerPID")
            window_lst[window_lst.size()-1].pid = myint;
        else if(key_str == "X")
            window_lst[window_lst.size()-1].x = myint;
        else if(key_str == "Y")
            window_lst[window_lst.size()-1].y = myint;
        else if(key_str == "Width")
            window_lst[window_lst.size()-1].width = myint;
        else if(key_str == "Height")
            window_lst[window_lst.size()-1].height = myint;
    }
    else if(id == CFDictionaryGetTypeID())
    {
        CFDictionaryRef elem = (CFDictionaryRef)value;
        CFDictionaryApplyFunction(elem, get_window_info, NULL);
        CFRelease(elem);
    } 
}

void fatal(const std::string &err)
{
    std::cout << err << std::endl;
    exit(1);
}

int main(int argc, char **argv)
{
    detect_window_below_cursor();

    CFMachPortRef event_tap;
    CGEventMask event_mask;
    CFRunLoopSourceRef run_loop_source;

    event_mask = ((1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventMouseMoved));
    event_tap = CGEventTapCreate( kCGSessionEventTap, kCGHeadInsertEventTap, 0, event_mask, cgevent_callback, NULL);

    if(event_tap && CGEventTapIsEnabled(event_tap))
        std::cout << "tapping keys.." << std::endl;
    else
        fatal("could not tap keys, try running as root");

    run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopCommonModes);
    CGEventTapEnable(event_tap, true);
    CFRunLoopRun();
    return 0;
}
