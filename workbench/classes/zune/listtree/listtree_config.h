#ifndef LISTTREE_CONFIG_H
#define LISTTREE_CONFIG_H

/* MUI 3.8's Listtree settings (its Listtree.mcp page), shared by
 * Listtree.mcc and Zune's Listtree.mcp. Numbers are stored as 4 bytes,
 * images and pens as spec strings. */
#define LTCFG_Right     0x80020010  /* image spec: closed node */
#define LTCFG_Down      0x80020011  /* image spec: open node */
#define LTCFG_Remember  0x80020012  /* closed nodes remember their children */
#define LTCFG_Style     0x80020013  /* LTSTYLE_... */
#define LTCFG_Space     0x80020014  /* node image to text, 0..16 */
#define LTCFG_Opened    0x80020015  /* "Display opened": scroll after opening */
#define LTCFG_Lines     0x80020016  /* pen spec */
#define LTCFG_Shadow    0x80020020  /* pen spec */

#define LTSTYLE_Normal   0
#define LTSTYLE_Inserted 1
#define LTSTYLE_Lines    2
#define LTSTYLE_Win95    3
#define LTSTYLE_Mac      4
#define LTSTYLE_Lines3D  5
#define LTSTYLE_Count    6

#define LTCFG_Right_Default     "1:17"
#define LTCFG_Down_Default      "1:23"
#define LTCFG_Lines_Default     "m5"
#define LTCFG_Shadow_Default    "m4"
#define LTCFG_Remember_Default  1
#define LTCFG_Opened_Default    1
#define LTCFG_Style_Default     LTSTYLE_Win95
#define LTCFG_Space_Default     6
#define LTCFG_Space_Max         16

#endif
