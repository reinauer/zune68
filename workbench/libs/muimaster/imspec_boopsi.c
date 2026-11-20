/* "3:<name>.image" image specs: an external BOOPSI image class, as the
 * MUIA_Image_Spec autodoc describes "3:". The class is a class library
 * file, e.g. MUI:Images/kmel/kmel_arrowdown.image: a name without a volume
 * is looked for under IMSPEC_EXTERNAL_PREFIX. The library is opened (one
 * open per spec that is set up), an object is made of its public class
 * (the file name, then the file name without ".image", as image class
 * libraries name their classes, then the class a ClassLibrary carries),
 * and both are given back at cleanup. imspec.c draws the object with
 * DrawImageState() at the size the object has been given.
 *
 * Sizes: image classes written for MUI answer a method of their own with
 * their minimum, maximum and default size and keep the render info for
 * MUI's pens (the kmel images, MUI 3.8's Misc/mui-stdcycle.image). Any
 * other class has the size its object has, and may be stretched on a free
 * axis (it is drawn with IA_Width and IA_Height set).
 *
 * Ported from OpenMUI (imspec_boopsi.c, Tobias Karlsson).
 */

#include <string.h>
#include <stdio.h>

#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>

#include "mui.h"
#include "support.h"
#include "imspec_intern.h"

extern struct ExecBase *SysBase;

/* The size method of image classes written for MUI: it fills in the
 * minimum, maximum and default width and height and answers with its own
 * id; mri is the render info the object draws with (for MUI's pens). */
#define BOOPSI_IMAGE_SIZES 0x80427157

struct BoopsiImageSizes
{
    STACKED ULONG MethodID;
    STACKED ULONG unused[2];
    STACKED LONG minw, minh, maxw, maxh, defw, defh;
    STACKED struct MUI_RenderInfo *mri;
};

static Object *new_image(Class *cl, CONST_STRPTR id, struct DrawInfo *dri)
{
    struct TagItem tags[] = {
        { SYSIA_DrawInfo, (IPTR)dri },
        { TAG_DONE, 0 }
    };

    return NewObjectA(cl, (ClassID)id, tags);
}

static UWORD clamp_size(LONG v, UWORD otherwise)
{
    return v <= 0 ? otherwise : v > MUI_MAXMAX ? MUI_MAXMAX : (UWORD)v;
}

/* The library of the file at path: the one already in memory under the
 * file's name (OpenLibrary() of a path would load a second copy), or the
 * file itself if it is there (OpenLibrary() would search LIBS: for the bare
 * name otherwise). */
static struct Library *open_image_library(CONST_STRPTR path)
{
    CONST_STRPTR name = FilePart((STRPTR)path);
    struct Library *lib = NULL;
    BPTR lock;

    Forbid();
    if (FindName(&SysBase->LibList, (STRPTR)name))
        lib = OpenLibrary((STRPTR)name, 0);
    Permit();
    if (!lib && (lock = Lock((STRPTR)path, ACCESS_READ)))
    {
        UnLock(lock);
        lib = OpenLibrary((STRPTR)path, 0);
    }
    return lib;
}

BOOL zune_boopsi_image_open(struct MUI_ImageSpec_intern *spec,
    struct MUI_RenderInfo *mri)
{
    CONST_STRPTR name = spec->u.boopsi.filename;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct BoopsiImageSizes msg;
    struct Library *lib = NULL;
    Object *o;
    UWORD *size = spec->u.boopsi.size;
    char path[256], id[64];
    APTR oldwin;
    int n;

    spec->u.boopsi.base = NULL;
    spec->u.boopsi.obj = NULL;
    /* Lock and LoadSeg need a process */
    if (!name || !name[0] || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
        return FALSE;

    /* no "please insert volume MUI" when MUI: is not assigned */
    oldwin = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    if (strchr(name, ':'))
        snprintf(path, sizeof(path), "%s", name);
    else
        snprintf(path, sizeof(path), "%s%s", IMSPEC_EXTERNAL_PREFIX, name);
    lib = open_image_library(path);
    me->pr_WindowPtr = oldwin;
    if (!lib)
        return FALSE;

    snprintf(id, sizeof(id), "%s", FilePart((STRPTR)name));
    o = new_image(NULL, id, mri->mri_DrawInfo);
    n = strlen(id);
    if (!o && n > 6 && !strcmp(id + n - 6, ".image"))
    {
        id[n - 6] = 0;
        o = new_image(NULL, id, mri->mri_DrawInfo);
    }
    if (!o && lib->lib_PosSize >= sizeof(struct ClassLibrary))
    {
        Class *cl = ((struct ClassLibrary *)lib)->cl_Class;

        if (cl && !((IPTR)cl & 1) && TypeOfMem(cl))
            o = new_image(cl, NULL, mri->mri_DrawInfo);
    }
    if (!o)
    {
        CloseLibrary(lib);
        return FALSE;
    }

    memset(&msg, 0, sizeof(msg));
    msg.MethodID = BOOPSI_IMAGE_SIZES;
    msg.mri = mri;
    if (DoMethodA(o, (Msg)&msg) == BOOPSI_IMAGE_SIZES)
    {
        size[0] = clamp_size(msg.minw, 1);
        size[1] = clamp_size(msg.minh, 1);
        size[2] = clamp_size(msg.maxw, MUI_MAXMAX);
        size[3] = clamp_size(msg.maxh, MUI_MAXMAX);
        size[4] = clamp_size(msg.defw, size[0]);
        size[5] = clamp_size(msg.defh, size[1]);
    }
    else
    {
        struct Image *im = (struct Image *)o;

        size[0] = size[4] = clamp_size(im->Width, 8);
        size[1] = size[5] = clamp_size(im->Height, 8);
        size[2] = size[3] = MUI_MAXMAX;
    }
    spec->u.boopsi.base = lib;
    spec->u.boopsi.obj = o;
    return TRUE;
}

void zune_boopsi_image_close(struct MUI_ImageSpec_intern *spec)
{
    if (spec->u.boopsi.obj)
        DisposeObject(spec->u.boopsi.obj);
    if (spec->u.boopsi.base)
        CloseLibrary(spec->u.boopsi.base);
    spec->u.boopsi.obj = NULL;
    spec->u.boopsi.base = NULL;
}
