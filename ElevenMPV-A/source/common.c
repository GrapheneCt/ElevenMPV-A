#include "common.h"

vita2d_pvf *font;
int SCE_CTRL_ENTER, SCE_CTRL_CANCEL;
SceUInt32 hold = 0;
SceUInt32 pressed = 0;
int position = 0;
int file_count = 0;
char cwd[512];
char root_path[8];