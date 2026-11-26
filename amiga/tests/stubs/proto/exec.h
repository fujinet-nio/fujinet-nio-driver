#ifndef NATIVE_TEST_PROTO_EXEC_H
#define NATIVE_TEST_PROTO_EXEC_H

#include <exec/io.h>

LONG OpenDevice(CONST_STRPTR name, ULONG unit, struct IORequest *request, ULONG flags);
void CloseDevice(struct IORequest *request);
LONG DoIO(struct IORequest *request);
void SendIO(struct IORequest *request);
struct IORequest *CheckIO(struct IORequest *request);
LONG WaitIO(struct IORequest *request);
LONG AbortIO(struct IORequest *request);
ULONG Wait(ULONG signals);
ULONG SetSignal(ULONG new_signals, ULONG signal_mask);
void Disable(void);
void Enable(void);
void ReplyMsg(struct Message *message);
void Cause(struct Interrupt *interrupt);
APTR AllocMem(ULONG bytes, ULONG flags);
void FreeMem(APTR memory, ULONG bytes);
void Remove(struct Node *node);
void AddTail(struct List *list, struct Node *node);

#endif
