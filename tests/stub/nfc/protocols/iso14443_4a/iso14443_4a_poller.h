#pragma once
#include <furi.h>
typedef enum { Iso14443_4aErrorNone } Iso14443_4aError;
Iso14443_4aError iso14443_4a_poller_send_block(void* p, BitBuffer* tx, BitBuffer* rx);
