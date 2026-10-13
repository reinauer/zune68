#ifndef ZUNE68_MUI5_FIXTURE_H
#define ZUNE68_MUI5_FIXTURE_H
#include <exec/ports.h>
#define MUI5_FIXTURE_PORT "ZUNE68.MUI5.FIXTURE"
#define MUI5_FIXTURE_MAGIC 0x5a354958UL
struct MUI5Fixture
{
    struct MsgPort port;
    ULONG magic;
    Object *application;
};
#endif
