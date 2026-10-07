/*
    Copyright (C) 2002, The AROS Development Team.
    All rights reserved.

*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**************************************************************************
 This file builds the libraries/mui.h file. It outputs it to stdout
 so you can redirect them

 Currently it merges some files but later version might do more
 things
**************************************************************************/
/* Shared across recursion; an include finishes using it before recursing. */
static char linebuf[8192];

/* array of already included files */
char **included;
int included_num;

static int need_to_be_included(char *filename)
{
    for (int i = 0; i < included_num; i++)
    {
        if (!strcmp(included[i], filename))
            return 0;
    }
    return 1;
}

static void readfile(FILE *in)
{
    while (fgets(linebuf, sizeof(linebuf), in))
    {
        size_t len = strlen(linebuf);

        /* A private macro may span several physical lines. Drop all of it
           so a trailing backslash cannot swallow the next public macro. */
        while (len >= 2 && linebuf[len - 2] == '\\')
        {
            if (len + 1 >= sizeof(linebuf) ||
                !fgets(linebuf + len, sizeof(linebuf) - len, in))
            {
                fprintf(stderr, "buildincludes: incomplete logical line\n");
                exit(20);
            }
            len = strlen(linebuf);
        }
        if (!strstr(linebuf, "PRIV"))
        {
            if (strchr(linebuf, '#') && strstr(linebuf, "include"))
            {
                char *start = strchr(linebuf, '"');
                if (start)
                {
                    char *end;
                    start++;
                    end = strchr(start, '"');
                    if (end)
                    {
                        *end = 0;
                        if (need_to_be_included(start))
                        {
                            FILE *in2;
                            if (!(included =
                                    realloc(included,
                                        sizeof(char *) * (included_num +
                                            1))))
                                return;
                            included[included_num++] = strdup(start);
                            if ((in2 = fopen(start, "r")))
                            {
                                readfile(in2);
                                fclose(in2);
                            }
                        }
                    }
                }
                else
                    printf("%s", linebuf);
            }
            else
                printf("%s", linebuf);
        }
    }
}

int main(void)
{

    /* Open the mui.h file */
    FILE *in = fopen("mui.h", "r");
    if (in)
    {
        readfile(in);
        fclose(in);
    }

    return 0;
}
