/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
*/

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"

/* #define MYDEBUG 1 */
#include "debug.h"

/* Whether a tag hands an object over to the object being made. */
static BOOL zune_child_tag(Tag t)
{
    switch (t)
    {
    case MUIA_Group_Child:
    case MUIA_Family_Child:
    case MUIA_Window_RootObject:
    case MUIA_Window_Menustrip:
    case MUIA_Application_Window:
    case MUIA_Application_Menustrip:
    case MUIA_Listview_List:
    case MUIA_Scrollgroup_Contents:
    case MUIA_Popstring_String:
    case MUIA_Popstring_Button:
    case MUIA_Popobject_Object:
        return TRUE;
    }
    return FALSE;
}

/* MUI's rule (Group.mui, MUIA_Group_Child): a failed MUI_NewObject() call
 * leaves no previously created object behind, however deep the failure.
 * A class whose OM_NEW fails before it reads its tags calls this to
 * dispose the objects the tag list hands over: a group's children, a
 * window's root object and menustrip, an application's windows and
 * menustrip, a listview's list, a scrollgroup's contents, a popup's
 * string, button and object. One that fails after reading them disposes
 * them in its OM_DISPOSE. Popstring passes its string and button to Group
 * as children with its own tag list after them (TAG_MORE), and Listview
 * its list, so the same object can appear twice: only its last
 * appearance disposes it. */
void zune_dispose_children(struct TagItem *tags)
{
    struct TagItem *tag, *rest, *t;

    while ((tag = NextTagItem(&tags)))
    {
        if (!zune_child_tag(tag->ti_Tag) || !tag->ti_Data)
            continue;
        rest = tag + 1;
        while ((t = NextTagItem(&rest)))
            if (t->ti_Data == tag->ti_Data && zune_child_tag(t->ti_Tag))
                break;
        if (!t)
            MUI_DisposeObject((Object *)tag->ti_Data);
    }
}

/*****************************************************************************

    NAME */
        AROS_LH2(Object *, MUI_NewObjectA,

/*  SYNOPSIS */
        AROS_LHA(CONST_STRPTR, classid,     A0),
        AROS_LHA(struct TagItem *, tags, A1),

/*  LOCATION */
        struct Library *, MUIMasterBase, 5, MUIMaster)

/*  FUNCTION
        Create an instance of the named MUI class with the supplied attributes.
        The class is looked up through MUI_GetClass(), then instantiated
        through intuition.library/NewObjectA().

    INPUTS
        classid - NUL-terminated class identifier. Built-in class names are
                  case-sensitive. Use the MUIC_* identifiers where available.
        tags - Attribute/value pairs terminated by TAG_DONE, or NULL for
               the class defaults. Attribute types and ownership are defined
               by the selected class.

    RESULT
        The new object, or NULL if the class lookup or construction fails.

    NOTES
        MUI_NewObject() is the variadic tag-list wrapper.

        Release an independently owned object with MUI_DisposeObject(). If
        ownership is transferred to a parent, follow that parent's disposal
        contract instead of disposing of the child twice.
        This function does not establish a separate MUI error code.

    SEE ALSO
        MUI_DisposeObject(), MUI_GetClass(),
        intuition.library/NewObjectA()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    Class  *cl;

    /* Without the class nothing reads the tag list, and its objects stay
     * the caller's: MUI 3.8 keeps them too (tests/native/fixpaths.c).
     * A built-in class that could not be made for lack of memory fails
     * like a class whose OM_NEW fails: it disposes them (MUI 3.8 makes
     * its classes when the library opens, and never gets here). */
    cl = MUI_GetClass(classid);
    if (cl)
    {
        Object *obj = NewObjectA(cl, NULL, tags);

        /* A built-in class stays alive while it has objects (expunge
         * checks cl_ObjectCount), so its objects keep no reference, like
         * MUI 3.9: one freed with DisposeObject() instead of
         * MUI_DisposeObject() must not keep the library resident. */
        if (obj && (cl->cl_Flags & CLF_INLIST))
            MUI_FreeClass(cl);
        if (obj) return obj;

        bug("*** Could not create object of %s\n", classid);
        MUI_FreeClass(cl);
    }
    else if (ZUNE_IsBuiltinClass(classid))
        zune_dispose_children(tags);
    else
        bug("*** Couldn't find %s\n", classid);

    return NULL;

    AROS_LIBFUNC_EXIT
} /* MUIA_NewObjectA */
