#ifndef CAPTURE_COMMON_H
#define CAPTURE_COMMON_H

#include "session.h"

void capture_prepare(const sniffer_session_t *session);
void capture_stop(void);
void capture_task(void);

#endif // CAPTURE_COMMON_H