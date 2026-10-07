/*
    Copyright (C) 2026, The AROS Development Team.
    All rights reserved.
*/

#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/muimaster.h>

#include <zune/prefswindow.h>

#include "zunestuff.h"

#include "../../classes/zune/prefswindow/prefswindow_private.h"
struct MUI_CustomClass *ClassPrefsWindow_CLASS;
static struct MUI_CustomClass *native_window;
Object *PrefsWindow__OM_NEW(Class *, Object *, struct opSet *);
IPTR PrefsWindow__OM_DISPOSE(Class *, Object *, Msg);
IPTR PrefsWindow__OM_SET(Class *, Object *, struct opSet *);
IPTR PrefsWindow__OM_GET(Class *, Object *, struct opGet *);
BOOPSI_DISPATCHER(IPTR, NativePrefsWindow, cl, obj, msg)
{
    switch (msg->MethodID) {
    case OM_NEW: return (IPTR)PrefsWindow__OM_NEW(cl, obj, (APTR)msg);
    case OM_DISPOSE: return PrefsWindow__OM_DISPOSE(cl, obj, msg);
    case OM_SET: return PrefsWindow__OM_SET(cl, obj, (APTR)msg);
    case OM_GET: return PrefsWindow__OM_GET(cl, obj, (APTR)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END


BOOPSI_DISPATCHER(IPTR, ClassPrefsWindow_Dispatcher, CLASS, self, message)
{
    switch (message->MethodID)
    {
    case MUIM_PrefsWindow_Test:
        main_test_pressed();
        return 0;

    case MUIM_PrefsWindow_Revert:
        main_revert_pressed();
        return 0;

    case MUIM_PrefsWindow_Save:
        main_save_pressed();
        return 0;

    case MUIM_PrefsWindow_Use:
        main_use_pressed();
        return 0;

    case MUIM_PrefsWindow_Cancel:
        main_cancel_pressed();
        return 0;

    default:
        return DoSuperMethodA(CLASS, self, message);
    }

    return 0;
}
BOOPSI_DISPATCHER_END


struct MUI_CustomClass *create_prefswindow_class(void)
{
    native_window = MUI_CreateCustomClass(NULL, MUIC_Window, NULL,
        sizeof(struct PrefsWindow_DATA), NativePrefsWindow);
    if (!native_window) return NULL;
    ClassPrefsWindow_CLASS = MUI_CreateCustomClass(NULL, NULL, native_window,
        0, ClassPrefsWindow_Dispatcher);
    if (!ClassPrefsWindow_CLASS) {
        MUI_DeleteCustomClass(native_window);
        native_window = NULL;
    }
    return ClassPrefsWindow_CLASS;
}

void delete_prefswindow_class(void)
{
    if (ClassPrefsWindow_CLASS)
        MUI_DeleteCustomClass(ClassPrefsWindow_CLASS);
    ClassPrefsWindow_CLASS = NULL;
    if (native_window) MUI_DeleteCustomClass(native_window);
    native_window = NULL;
}
