/*
    Copyright (C) 2002-2020, The AROS Development Team. All rights reserved.
*/

#include <string.h>
#include <stdio.h>

#include <intuition/classes.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>


#include "mui.h"
#include "support.h"
#include "support_classes.h"
#include "muimaster_intern.h"

/*#define MYDEBUG*/
#include "debug.h"

#if !defined(__AROS__) && !defined(__amigaos4__) && !defined(__MAXON__)
#include "classes/area.h"

static IPTR builtinDispatcher(Class *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"));

static IPTR __attribute__((noinline)) builtinDraw(Class *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    typedef IPTR (*Dispatcher)(Class * __asm("a0"),
        Object * __asm("a2"), Msg __asm("a1"), APTR __asm("a6"));
    Dispatcher dispatch = (Dispatcher)cl->cl_Dispatcher.h_SubEntry;
    IPTR result = dispatch(cl, obj, msg, cl->cl_Dispatcher.h_Data);

    /* Enabled controls take no class walk. Only the outermost built-in
     * draws the pattern, after its inherited content has been painted. */
    if (muiAreaData(obj)->mad_DisableCount)
    {
        Class *derived = OCLASS(obj);

        while (derived && derived->cl_Dispatcher.h_Entry !=
                (HOOKFUNC)builtinDispatcher)
            derived = derived->cl_Super;
        if (derived == cl)
            ZuneDrawDisabled(obj);
    }
    return result;
}

static IPTR builtinDispatcher(Class *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    /* Select before dispatch: methods such as Dispose can invalidate the
     * message storage. Other methods retain the ordinary trampoline path. */
    if (msg->MethodID == MUIM_Draw)
        return builtinDraw(cl, obj, msg);
    {
        typedef IPTR (*Dispatcher)(Class * __asm("a0"),
            Object * __asm("a2"), Msg __asm("a1"), APTR __asm("a6"));
        Dispatcher dispatch = (Dispatcher)cl->cl_Dispatcher.h_SubEntry;
        return dispatch(cl, obj, msg, cl->cl_Dispatcher.h_Data);
    }
}
#endif

extern const struct __MUIBuiltinClass _MUI_Settings_desc;

static const struct __MUIBuiltinClass *const builtins[] = {
    &_MUI_Notify_desc,
    &_MUI_Family_desc,
    &_MUI_Application_desc,
    &_MUI_Window_desc,
    &_MUI_Area_desc,
    &_MUI_Rectangle_desc,
    &_MUI_Group_desc,
    &_MUI_Image_desc,
    &_MUI_Configdata_desc,
    &_MUI_Text_desc,
    &_MUI_Numeric_desc,
    &_MUI_Slider_desc,
    &_MUI_String_desc,
    ZUNE_BOOPSI_DESC & _MUI_Prop_desc,
    &_MUI_Scrollbar_desc,
    &_MUI_Register_desc,
    &_MUI_Menuitem_desc,
    &_MUI_Menu_desc,
    &_MUI_Menustrip_desc,
    ZUNE_VIRTGROUP_DESC ZUNE_SCROLLGROUP_DESC & _MUI_Scrollbutton_desc,
    &_MUI_Semaphore_desc,
    &_MUI_Dataspace_desc,
    &_MUI_Bitmap_desc,
    &_MUI_Bodychunk_desc,
    &_MUI_ChunkyImage_desc,
    &_MUI_Cycle_desc,
    &_MUI_Popstring_desc,
    &_MUI_Listview_desc,
    &_MUI_List_desc,
    ZUNE_FLOATTEXT_DESC
    ZUNE_POPASL_DESC & _MUI_Popobject_desc,
    ZUNE_GAUGE_DESC
        ZUNE_ABOUTMUI_DESC
        (&_MUI_Settings_desc),
        ZUNE_SETTINGSGROUP_DESC
        ZUNE_IMAGEADJUST_DESC
        ZUNE_POPIMAGE_DESC
        ZUNE_SCALE_DESC
        ZUNE_RADIO_DESC
        ZUNE_ICONLISTVIEW_DESC
        ZUNE_BALANCE_DESC
        ZUNE_COLORFIELD_DESC
        ZUNE_COLORADJUST_DESC
        ZUNE_IMAGEDISPLAY_DESC
        ZUNE_PENDISPLAY_DESC
        ZUNE_PENADJUST_DESC ZUNE_POPPEN_DESC & _MUI_Mccprefs_desc,
    ZUNE_FRAMEDISPLAY_DESC
        ZUNE_POPFRAME_DESC
        ZUNE_FRAMEADJUST_DESC
        ZUNE_VOLUMELIST_DESC
        ZUNE_DIRLIST_DESC
        ZUNE_NUMERICBUTTON_DESC
        ZUNE_POPLIST_DESC
        ZUNE_CRAWLING_DESC
        ZUNE_POPSCREEN_DESC
        ZUNE_LEVELMETER_DESC
        ZUNE_KNOB_DESC
        ZUNE_DTPIC_DESC
        ZUNE_PALETTE_DESC
        ZUNE_PANEL_DESC
        ZUNE_PANELGROUP_DESC
        ZUNE_DRAGHANDLE_DESC
        ZUNE_PANELTITLE_DESC
};

Class *ZUNE_GetExternalClass(CONST_STRPTR classname,
    struct Library *MUIMasterBase)
{
    struct Library *mcclib = NULL;
    struct MUI_CustomClass *mcc = NULL;
    CONST_STRPTR const *pathptr;
    TEXT s[255];

    static CONST_STRPTR const searchpaths[] = {
        "Zune/%s",
        "Classes/Zune/%s",
        "MUI/%s",
        NULL,
    };

    for (pathptr = searchpaths; *pathptr; pathptr++)
    {
        snprintf(s, 255, *pathptr, classname);

        D(bug("Trying opening of %s\n", s));

        if ((mcclib = OpenLibrary(s, 0)))
        {
            D(bug("Calling MCC Query. Librarybase at 0x%p\n", mcclib));

            mcc = MCC_Query(0);
            if (!mcc)
                mcc = MCC_Query(1);     /* MCP? */

            if (mcc)
            {
                if (mcc->mcc_Class)
                {
                    mcc->mcc_Module = mcclib;
                    D(bug("Successfully opened %s as external class\n",
                            classname));

                    return mcc->mcc_Class;
                }
            }

            CloseLibrary(mcclib);
        }
    }

    D(bug("Failed to open external class %s\n", classname));
    return NULL;
}

/**************************************************************************/
static Class *ZUNE_FindBuiltinClass(CONST_STRPTR classid, struct Library *MUIMasterBase)
{
    struct MUIMasterBase_intern *intZuneBase = (struct MUIMasterBase_intern *)MUIMasterBase;
    Class *cl = NULL, *cl2;

    ForeachNode(&intZuneBase->BuiltinClasses, cl2)
    {
        if (!strcmp(cl2->cl_ID, classid))
        {
            cl = cl2;
            break;
        }
    }

    return cl;
}

static Class *ZUNE_MakeBuiltinClass(CONST_STRPTR classid,
    struct Library *MUIMasterBase)
{
    int i;
    Class *cl = NULL;

    D(bug("Makeing Builtinclass %s\n", classid));

    for (i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
    {
        if (!strcmp(builtins[i]->name, classid))
        {
            Class *supercl;
            ClassID superclid;

            /* Cached classes do not own library opens. Expunge checks
               references, objects and subclasses before reclaiming them. */
            if (strcmp(builtins[i]->supername, ROOTCLASS) == 0)
            {
                superclid = ROOTCLASS;
                supercl = NULL;
            }
            else
            {
                superclid = NULL;
                supercl = MUI_GetClass(builtins[i]->supername);

                if (!supercl)
                    break;
            }

            cl = MakeClass(builtins[i]->name, superclid, supercl,
                builtins[i]->datasize, 0);
            /* MakeClass owns the subclass link on success. The temporary
               lookup reference is no longer needed, including on failure. */
            if (supercl)
                MUI_FreeClass(supercl);
            if (cl)
            {
#if defined(__MAXON__) || defined(__amigaos4__)
                cl->cl_Dispatcher.h_Entry = builtins[i]->dispatcher;
#else
#ifdef __AROS__
                cl->cl_Dispatcher.h_Entry = (HOOKFUNC) metaDispatcher;
#else
                cl->cl_Dispatcher.h_Entry = (HOOKFUNC) builtinDispatcher;
#endif
                cl->cl_Dispatcher.h_SubEntry = builtins[i]->dispatcher;
#endif
                /* Use this as a reference counter */
                cl->cl_Dispatcher.h_Data = MUIMasterBase;
                cl->cl_UserData = 0;
            }

            break;
        }
    }

    return cl;
}

Class *ZUNE_GetBuiltinClass(CONST_STRPTR id, struct Library *base)
{
    Class *cl;
    ObtainSemaphore(&MUIMB(base)->ZuneSemaphore);
    cl = ZUNE_FindBuiltinClass(id, base);
    if (!cl) {
        cl = ZUNE_MakeBuiltinClass(id, base);
        if (cl) ZUNE_AddBuiltinClass(cl, base);
    }
    if (cl) ++cl->cl_UserData;
    ReleaseSemaphore(&MUIMB(base)->ZuneSemaphore);
    return cl;
}

/* Reclaim cached classes only at expunge, never from disposal callbacks.
 * Classes enter the list after their superclasses, so walk backwards.
 * Keep the library resident if any reference, object or subclass survives.
 */
BOOL ZUNE_FreeBuiltinClasses(struct Library *mb)
{
    struct MUIMasterBase_intern *base = (struct MUIMasterBase_intern *)mb;
    struct MinNode *node, *previous;
    BOOL empty;

    /* Expunge runs under Exec's Forbid. Do not wait and allow another
       task to reopen the library after LibExpunge checked its open count. */
    if (!AttemptSemaphore(&base->ZuneSemaphore))
        return FALSE;
    node = base->BuiltinClasses.mlh_TailPred;
    while (node->mln_Pred)
    {
        Class *cl = (Class *)node;

        previous = node->mln_Pred;
        if (!cl->cl_UserData && !cl->cl_ObjectCount && !cl->cl_SubclassCount)
        {
            ZUNE_RemoveBuiltinClass(cl, mb);
            if (!FreeClass(cl))
            {
                /* Keep a refused class reachable in its original position. */
                Insert((struct List *)&base->BuiltinClasses,
                    (struct Node *)cl, (struct Node *)previous);
                cl->cl_Flags |= CLF_INLIST;
            }
        }
        node = previous;
    }
    empty = base->BuiltinClasses.mlh_Head->mln_Succ == NULL;
    ReleaseSemaphore(&base->ZuneSemaphore);
    return empty;
}

/*
 * metaDispatcher - puts h_Data in A6 and calls real dispatcher
 */

#ifdef __AROS__
AROS_UFH3(IPTR, metaDispatcher,
    AROS_UFHA(struct IClass *, cl, A0),
    AROS_UFHA(Object *, obj, A2),
    AROS_UFHA(Msg, msg, A1))
{
    AROS_USERFUNC_INIT

    return AROS_UFC4(IPTR, cl->cl_Dispatcher.h_SubEntry,
        AROS_UFPA(Class *, cl, A0),
        AROS_UFPA(Object *, obj, A2),
        AROS_UFPA(Msg, msg, A1),
        AROS_UFPA(APTR, cl->cl_Dispatcher.h_Data, A6));

    AROS_USERFUNC_EXIT
}

#else
IPTR metaDispatcher(Class *cl __asm("a0"), Object *obj __asm("a2"),
    Msg msg __asm("a1"))
{
    typedef IPTR (*Dispatcher)(Class * __asm("a0"),
        Object * __asm("a2"), Msg __asm("a1"), APTR __asm("a6"));
    Dispatcher dispatch = (Dispatcher)cl->cl_Dispatcher.h_SubEntry;
    return dispatch(cl, obj, msg, cl->cl_Dispatcher.h_Data);
}
#endif
