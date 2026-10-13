#ifndef _TITLE_PRIVATE_H_
#define _TITLE_PRIVATE_H_

/*** Instance data **********************************************************/
struct Title_DATA
{
    struct Hook *layout_hook;
    struct MUI_EventHandlerNode ehn;
    LONG activetab;
    ULONG position, onlastclose;
    BOOL clickable, closable, newable, sortable;
    LONG pressed, first, last;
    BOOL overflow;
    Object *page_menu;
};

#endif /* _TITLE_PRIVATE_H_ */
