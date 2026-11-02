#ifndef _MUI_CLASSES_TEXTINPUT_H
#define _MUI_CLASSES_TEXTINPUT_H

/* Textinput API. As in MUI 4 and later, String.mui implements it, and
 * Textinput.mcc/Textinputscroll.mcc are thin subclasses. The guard is the
 * one of the original header, so including both does no harm. */
#ifndef TEXTINPUT_MCC_H
#define TEXTINPUT_MCC_H

/*** Names ******************************************************************/
#define MUIC_Textinput        "Textinput.mcc"
#define MUIC_Textinputscroll  "Textinputscroll.mcc"
#define TextinputObject       MUI_NewObject(MUIC_Textinput
#define TextinputscrollObject MUI_NewObject(MUIC_Textinputscroll

#define MCC_TI_TAGBASE ((TAG_USER) | ((1307 << 16) + 0x712))
#define MCC_TI_ID(x)   (MCC_TI_TAGBASE + (x))

#define MCC_Textinput_Version  29
#define MCC_Textinput_Revision 1

/*** Methods ****************************************************************/
#define MUIM_Textinput_ExternalEdit     MCC_TI_ID(1)
#define MUIM_Textinput_SaveToFile       MCC_TI_ID(5)
#define MUIM_Textinput_LoadFromFile     MCC_TI_ID(6)
#define MUIM_Textinput_DoRevert         MCC_TI_ID(8)
#define MUIM_Textinput_DoDelLine        MCC_TI_ID(9)
#define MUIM_Textinput_DoMarkStart      MCC_TI_ID(10)
#define MUIM_Textinput_DoMarkAll        MCC_TI_ID(11)
#define MUIM_Textinput_DoCut            MCC_TI_ID(12)
#define MUIM_Textinput_DoCopy           MCC_TI_ID(13)
#define MUIM_Textinput_DoPaste          MCC_TI_ID(14)
#define MUIM_Textinput_AppendText       MCC_TI_ID(15)
#define MUIM_Textinput_DoToggleWordwrap MCC_TI_ID(19)
#define MUIM_Textinput_Acknowledge      MCC_TI_ID(20)
#define MUIM_Textinput_TranslateEvent   MCC_TI_ID(21)
#define MUIM_Textinput_InsertText       MCC_TI_ID(22)
#define MUIM_Textinput_DoLeft           MCC_TI_ID(23)
#define MUIM_Textinput_DoRight          MCC_TI_ID(24)
#define MUIM_Textinput_DoUp             MCC_TI_ID(25)
#define MUIM_Textinput_DoDown           MCC_TI_ID(26)
#define MUIM_Textinput_DoLineStart      MCC_TI_ID(27)
#define MUIM_Textinput_DoLineEnd        MCC_TI_ID(28)
#define MUIM_Textinput_DoTop            MCC_TI_ID(29)
#define MUIM_Textinput_DoBottom         MCC_TI_ID(30)
#define MUIM_Textinput_DoPageUp         MCC_TI_ID(31)
#define MUIM_Textinput_DoPageDown       MCC_TI_ID(32)
#define MUIM_Textinput_DoPopup          MCC_TI_ID(33)
#define MUIM_Textinput_DoPrevWord       MCC_TI_ID(34)
#define MUIM_Textinput_DoNextWord       MCC_TI_ID(35)
#define MUIM_Textinput_DoDel            MCC_TI_ID(36)
#define MUIM_Textinput_DoDelEOL         MCC_TI_ID(37)
#define MUIM_Textinput_DoBS             MCC_TI_ID(38)
#define MUIM_Textinput_DoBSSOL          MCC_TI_ID(39)
#define MUIM_Textinput_DoubleClick      MCC_TI_ID(42)
#define MUIM_Textinput_DoBSWord         MCC_TI_ID(43)
#define MUIM_Textinput_DoDelWord        MCC_TI_ID(44)
#define MUIM_Textinput_DoInsertFile     MCC_TI_ID(45)
#define MUIM_Textinput_InsertFromFile   MCC_TI_ID(46)
#define MUIM_Textinput_HandleChar       MCC_TI_ID(47)
#define MUIM_Textinput_HandleURL        MCC_TI_ID(48)
#define MUIM_Textinput_DoToggleCase     MCC_TI_ID(51)
#define MUIM_Textinput_DoToggleCaseEOW  MCC_TI_ID(52)
#define MUIM_Textinput_DoIncrementDec   MCC_TI_ID(53)
#define MUIM_Textinput_DoDecrementDec   MCC_TI_ID(54)
#define MUIM_Textinput_DoUndo           MCC_TI_ID(56)
#define MUIM_Textinput_DoRedo           MCC_TI_ID(57)
#define MUIM_Textinput_DoTab            MCC_TI_ID(58)
#define MUIM_Textinput_DoNextGadget     MCC_TI_ID(59)
#define MUIM_Textinput_DoSetBookmark1   MCC_TI_ID(60)
#define MUIM_Textinput_DoSetBookmark2   MCC_TI_ID(61)
#define MUIM_Textinput_DoSetBookmark3   MCC_TI_ID(62)
#define MUIM_Textinput_DoGotoBookmark1  MCC_TI_ID(63)
#define MUIM_Textinput_DoGotoBookmark2  MCC_TI_ID(64)
#define MUIM_Textinput_DoGotoBookmark3  MCC_TI_ID(65)
#define MUIM_Textinput_DoCutLine        MCC_TI_ID(66)
#define MUIM_Textinput_DoCopyCut        MCC_TI_ID(67)

struct MUIP_Textinput_SaveToFile     { STACKED ULONG MethodID; STACKED STRPTR filename; };
struct MUIP_Textinput_LoadFromFile   { STACKED ULONG MethodID; STACKED STRPTR filename; };
struct MUIP_Textinput_AppendText     { STACKED ULONG MethodID; STACKED STRPTR text; STACKED LONG len; };
struct MUIP_Textinput_Acknowledge    { STACKED ULONG MethodID; STACKED STRPTR contents; };
struct MUIP_Textinput_TranslateEvent { STACKED ULONG MethodID; STACKED struct InputEvent *ie; STACKED STRPTR mappedstring; STACKED ULONG *mappedlen; };
struct MUIP_Textinput_InsertText     { STACKED ULONG MethodID; STACKED STRPTR text; STACKED LONG len; };
struct MUIP_Textinput_DoubleClick    { STACKED ULONG MethodID; STACKED ULONG xp; STACKED ULONG yp; STACKED ULONG cnt; };
struct MUIP_Textinput_InsertFromFile { STACKED ULONG MethodID; STACKED STRPTR filename; };
struct MUIP_Textinput_HandleChar     { STACKED ULONG MethodID; STACKED ULONG ch; STACKED ULONG quiet; };
struct MUIP_Textinput_HandleURL      { STACKED ULONG MethodID; STACKED STRPTR url; };

/*** Attributes *************************************************************/
#define MUIA_Textinput_Multiline          MCC_TI_ID(100) /* i.g BOOL   */
#define MUIA_Textinput_MaxLen             MCC_TI_ID(101) /* i.g ULONG  */
#define MUIA_Textinput_MaxLines           MCC_TI_ID(102) /* i.g ULONG  */
#define MUIA_Textinput_AutoExpand         MCC_TI_ID(103) /* isg BOOL   */
#define MUIA_Textinput_Contents           MCC_TI_ID(104) /* isg STRPTR */
#define MUIA_Textinput_Blinkrate          MCC_TI_ID(108) /* isg ULONG  */
#define MUIA_Textinput_Cursorstyle        MCC_TI_ID(109) /* isg ULONG  */
#define MUIA_Textinput_AdvanceOnCR        MCC_TI_ID(110) /* isg BOOL   */
#define MUIA_Textinput_TmpExtension       MCC_TI_ID(111) /* isg STRPTR */
#define MUIA_Textinput_Quiet              MCC_TI_ID(112) /* .sg BOOL   */
#define MUIA_Textinput_Acknowledge        MCC_TI_ID(113) /* ..g STRPTR */
#define MUIA_Textinput_Integer            MCC_TI_ID(114) /* isg ULONG  */
#define MUIA_Textinput_MinVersion         MCC_TI_ID(115) /* i.. ULONG  */
#define MUIA_Textinput_DefaultPopup       MCC_TI_ID(117) /* i.. BOOL   */
#define MUIA_Textinput_WordWrap           MCC_TI_ID(118) /* isg ULONG  */
#define MUIA_Textinput_IsNumeric          MCC_TI_ID(119) /* isg BOOL   */
#define MUIA_Textinput_MinVal             MCC_TI_ID(120) /* isg ULONG  */
#define MUIA_Textinput_MaxVal             MCC_TI_ID(121) /* isg ULONG  */
#define MUIA_Textinput_AcceptChars        MCC_TI_ID(122) /* isg STRPTR */
#define MUIA_Textinput_RejectChars        MCC_TI_ID(123) /* isg STRPTR */
#define MUIA_Textinput_Changed            MCC_TI_ID(124) /* .sg BOOL   */
#define MUIA_Textinput_AttachedList       MCC_TI_ID(125) /* isg Object * */
#define MUIA_Textinput_RemainActive       MCC_TI_ID(126) /* isg BOOL   */
#define MUIA_Textinput_CursorPos          MCC_TI_ID(127) /* .sg ULONG  */
#define MUIA_Textinput_Secret             MCC_TI_ID(128) /* isg BOOL   */
#define MUIA_Textinput_Lines              MCC_TI_ID(129) /* ..g ULONG  */
#define MUIA_Textinput_Editable           MCC_TI_ID(130) /* isg BOOL   */
#define MUIA_Textinputscroll_UseWinBorder MCC_TI_ID(131) /* i.. BOOL   */
#define MUIA_Textinput_IsOld              MCC_TI_ID(132) /* isg BOOL   */
#define MUIA_Textinput_MarkStart          MCC_TI_ID(133) /* isg ULONG  */
#define MUIA_Textinput_MarkEnd            MCC_TI_ID(134) /* isg ULONG  */
#define MUIA_Textinputscroll_VertScrollerOnly MCC_TI_ID(135) /* i.. BOOL */
#define MUIA_Textinput_NoInput            MCC_TI_ID(136) /* i.g BOOL   */
#define MUIA_Textinput_SetMin             MCC_TI_ID(137) /* isg BOOL   */
#define MUIA_Textinput_SetMax             MCC_TI_ID(138) /* isg BOOL   */
#define MUIA_Textinput_SetVMax            MCC_TI_ID(139) /* isg BOOL   */
#define MUIA_Textinput_Styles             MCC_TI_ID(140) /* isg ULONG  */
#define MUIA_Textinput_PreParse           MCC_TI_ID(141) /* isg STRPTR */
#define MUIA_Textinput_Format             MCC_TI_ID(142) /* i.g ULONG  */
#define MUIA_Textinput_SetVMin            MCC_TI_ID(143) /* isg BOOL   */
#define MUIA_Textinput_HandleURLHook      MCC_TI_ID(144) /* isg struct Hook * */
#define MUIA_Textinput_Tabs               MCC_TI_ID(145) /* i.. ULONG  */
#define MUIA_Textinput_TabLen             MCC_TI_ID(146) /* i.. ULONG  */
#define MUIA_Textinput_Bookmark1          MCC_TI_ID(147) /* isg ULONG  */
#define MUIA_Textinput_Bookmark2          MCC_TI_ID(148) /* isg ULONG  */
#define MUIA_Textinput_Bookmark3          MCC_TI_ID(149) /* isg ULONG  */
#define MUIA_Textinput_CursorSize         MCC_TI_ID(150) /* isg ULONG  */
#define MUIA_Textinput_TopLine            MCC_TI_ID(151) /* isg ULONG  */
#define MUIA_Textinput_Font               MCC_TI_ID(152) /* isg ULONG  */
#define MUIA_Textinput_SuggestParse       MCC_TI_ID(153) /* isg ULONG  */
#define MUIA_Textinput_ProhibitParse      MCC_TI_ID(154) /* isg ULONG  */
#define MUIA_Textinput_NoCopy             MCC_TI_ID(155) /* isg ULONG  */
#define MUIA_Textinput_MinimumWidth       MCC_TI_ID(156) /* i.g ULONG  */
#define MUIA_Textinput_ResetMarkOnCursor  MCC_TI_ID(157) /* isg BOOL   */
#define MUIA_Textinput_NoExtraSpacing     MCC_TI_ID(158) /* isg BOOL   */
#define MUIA_Textinputscroll_VertBar      MCC_TI_ID(159) /* i.. APTR   */
#define MUIA_Textinputscroll_HorizBar     MCC_TI_ID(160) /* i.. APTR   */

/*** Values *****************************************************************/
#define MUIV_Textinput_ParseB_URL      0
#define MUIV_Textinput_ParseB_Misspell 1
#define MUIV_Textinput_ParseF_URL      (1 << MUIV_Textinput_ParseB_URL)
#define MUIV_Textinput_ParseF_Misspell (1 << MUIV_Textinput_ParseB_Misspell)

#define MUIV_Textinput_Tabs_Ignore 0
#define MUIV_Textinput_Tabs_Spaces 1
#define MUIV_Textinput_Tabs_Disk   2
#define MUIV_Textinput_Tabs_Tabs   3

#define MUIV_Textinput_NoMark ((ULONG)~0)

#define MUIV_Textinput_Styles_None  0
#define MUIV_Textinput_Styles_MUI   1
#define MUIV_Textinput_Styles_IRC   2
#define MUIV_Textinput_Styles_Email 3
#define MUIV_Textinput_Styles_HTML  4

#define MUIV_Textinput_Format_Left   0
#define MUIV_Textinput_Format_Center 1
#define MUIV_Textinput_Format_Centre 1
#define MUIV_Textinput_Format_Right  2

#define MUIV_Textinput_Font_Normal 0
#define MUIV_Textinput_Font_Fixed  1

#endif /* TEXTINPUT_MCC_H */
#endif /* _MUI_CLASSES_TEXTINPUT_H */
