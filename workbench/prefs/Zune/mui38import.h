#ifndef ZUNE_MUI38IMPORT_H
#define ZUNE_MUI38IMPORT_H

#include <exec/types.h>
#include <intuition/classusr.h>

/* MUI 3.8's settings drawer; its files are only read */
#define MUI38_DRAWER "ENVARC:MUI"

/* The name of MUI 3.8's global settings file in MUI38_DRAWER, if it has
 * one */
BOOL mui38_global_file(char *buf, LONG size);

/* Whether a file name is that of MUI 3.8's global settings file */
BOOL mui38_is_global_name(CONST_STRPTR name);

/* Read a MUI 3.8 settings file into a Configdata object (what it had
 * before is cleared). FALSE if it is no settings file. */
BOOL mui38_load(CONST_STRPTR from, Object *configdata);

/* Every program's own MUI 3.8 settings file in drawer that Zune has none
 * of yet, converted into ENVARC:zune and ENV:zune. Returns how many. */
LONG mui38_import_programs(CONST_STRPTR drawer);

/* No Zune settings yet, and MUI 3.8 settings to import */
BOOL mui38_first_run(void);

#endif
