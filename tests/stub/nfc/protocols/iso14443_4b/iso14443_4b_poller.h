#pragma once
#include <furi.h>
typedef enum { Iso14443_4bErrorNone } Iso14443_4bError;
Iso14443_4bError iso14443_4b_poller_send_block(void* p, BitBuffer* tx, BitBuffer* rx);
