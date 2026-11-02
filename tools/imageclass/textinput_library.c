/* Textinput.mcc and Textinputscroll.mcc. As in MUI 4 and later, String.mui
 * implements the Textinput API; these classes only provide the names and
 * the version. Build with TEXTINPUTSCROLL for Textinputscroll, a group
 * around one String object. */
#include <libraries/mui.h>
#include <mcc_common.h>
#include <SDI_hook.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <stdarg.h>

#define DEBUG 0
#define D(...) ((void)0)
#define E(...) ((void)0)
#include "shellstart.c"

struct Data
{
    Object *string;
};

#ifdef TEXTINPUTSCROLL
#define CLASS MUIC_Textinputscroll
#define SUPERCLASS MUIC_Group
#else
#define CLASS MUIC_Textinput
#define SUPERCLASS MUIC_String
#endif
#define INSTDATA Data
#define VERSION 29
#define REVISION 1
#define USERLIBID CLASS " 29.1 (02.11.2026) Zune68"
#define MASTERVERSION 19
#define MIN_STACKSIZE 16384
#define SetupDebug() ((void)0)
#define CleanupDebug() ((void)0)
#include "mccinit.c"

#ifdef TEXTINPUTSCROLL
static Object *DoSuperNew(struct IClass *cl, Object *obj, ...)
{
    va_list args;
    struct opSet message;
    Object *result;

    va_start(args, obj);
    message.MethodID = OM_NEW;
    message.ops_AttrList = (struct TagItem *)args;
    message.ops_GInfo = NULL;
    result = (Object *)DoSuperMethodA(cl, obj, (Msg)&message);
    va_end(args);
    return result;
}

static BOOL IsStringAttribute(ULONG id)
{
    switch (id)
    {
    case MUIA_String_Contents:
    case MUIA_String_BufferPos:
    case MUIA_String_Acknowledge:
    case MUIA_String_Integer:
    case MUIA_String_Secret:
    case MUIA_String_MaxLen:
        return TRUE;
    }
    return id >= MUIA_Textinput_Multiline && id <= MUIA_Textinput_NoExtraSpacing;
}

static IPTR ScrollNew(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Data *data;
    Object *string;

    /* Frame, background, font and cycle chain belong to the String */
    string = MUI_NewObject(MUIC_String, MUIA_Frame, MUIV_Frame_String,
        TAG_MORE, (IPTR)msg->ops_AttrList);
    if (!string)
        return 0;
    obj = (Object *)DoSuperNew(cl, obj, MUIA_Group_Horiz, TRUE,
        MUIA_Group_Spacing, 0, MUIA_Group_Child, (IPTR)string, TAG_DONE);
    if (!obj)
        return 0;
    data = INST_DATA(cl, obj);
    data->string = string;
    DoMethod(string, MUIM_Notify, MUIA_Textinput_Contents, MUIV_EveryTime,
        (IPTR)obj, 3, MUIM_Set, MUIA_Textinput_Contents, MUIV_TriggerValue);
    DoMethod(string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
        (IPTR)obj, 3, MUIM_Set, MUIA_String_Acknowledge, MUIV_TriggerValue);
    return (IPTR)obj;
}

static IPTR ScrollSet(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Data *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList, *tag;
    BOOL forward = FALSE;

    while ((tag = NextTagItem(&tags)))
        if (IsStringAttribute(tag->ti_Tag) || tag->ti_Tag == MUIA_Disabled)
            forward = TRUE;
    if (forward)
        SetAttrsA(data->string, msg->ops_AttrList);
    return DoSuperMethodA(cl, obj, (Msg)msg);
}
#endif

DISPATCHER(_Dispatcher)
{
#ifdef TEXTINPUTSCROLL
    struct Data *data;

    switch (msg->MethodID)
    {
    case OM_NEW:
        return ScrollNew(cl, obj, (APTR)msg);
    case OM_SET:
        return ScrollSet(cl, obj, (APTR)msg);
    case OM_GET:
        data = INST_DATA(cl, obj);
        if (IsStringAttribute(((struct opGet *)msg)->opg_AttrID))
            return DoMethodA(data->string, msg);
        break;
    case MUIM_GoActive:
        data = INST_DATA(cl, obj);
        if (_win(obj))
            set(_win(obj), MUIA_Window_ActiveObject, data->string);
        return 0;
    default:
        if (msg->MethodID >= MUIM_Textinput_ExternalEdit
            && msg->MethodID <= MUIM_Textinput_DoCopyCut)
        {
            data = INST_DATA(cl, obj);
            return DoMethodA(data->string, msg);
        }
    }
#endif
    return DoSuperMethodA(cl, obj, msg);
}
