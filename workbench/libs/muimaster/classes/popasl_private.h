#ifndef _POPASL_PRIVATE_H_
#define _POPASL_PRIVATE_H_

#include <exec/types.h>
#include <utility/hooks.h>
#include <utility/tagitem.h>

/*** Instance data **********************************************************/
struct Popasl_DATA
{
    int type;
    APTR asl_req;

    struct Hook open_hook;
    struct Hook close_hook;
    struct Hook *start_hook, *stop_hook;

    struct TagItem tag_list[20]; /* According to docs we need at least 16 */

    struct Process *asl_proc;   /* set until the Close hook has run */

    /* The worker process (Asl_Entry): TRUE until it has finished with the
     * requester; OM_DISPOSE sets asl_waiter and waits for it. */
    BOOL asl_running;
    struct Task *asl_waiter;
    Object *asl_app;            /* where the worker pushes the Close */
};

#endif /* _POPASL_PRIVATE_H_ */
