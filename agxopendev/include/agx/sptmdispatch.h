#ifndef AGX_SPTMDISPATCH_H
#define AGX_SPTMDISPATCH_H

#include "agx/types.h"
#include "agx/sptmregisters.h"

typedef struct __attribute__((aligned(16))) agxVec128 {
    u64 lo;
    u64 hi;
} agxVec128;

typedef struct sptmPerCpuState {
    u8 reserved0x000[0x450];
    u64 savedX0;
    u64 savedX1;
    u64 savedX2;
    u64 savedX3;
    u64 savedX4;
    u64 savedX5;
    u64 savedX6;
    u64 savedX7;
    u64 savedX8;
    u64 savedX9;
    u64 savedX10;
    u64 savedX11;
    u64 savedX12;
    u64 savedX13;
    u64 savedX14;
    u64 savedX15;
    u64 savedX16;
    u64 savedX17;
    u64 savedX18;
    u64 savedX19;
    u64 savedX20;
    u64 savedX21;
    u64 savedX22;
    u64 savedX23;
    u64 savedX24;
    u64 savedX25;
    u64 savedX26;
    u64 savedX27;
    u64 savedX28;
    u64 savedX29;
    u64 savedX30;
    u64 savedElrGl1;
    u32 savedSpsrGl1;
    u32 padA;
    u64 savedFarGl1;
    u64 savedEsrGl1;
    u64 savedSpGl12;
    u8 padB[8];
    agxVec128 savedQ0;
    agxVec128 savedQ1;
    agxVec128 savedQ2;
    agxVec128 savedQ3;
    agxVec128 savedQ4;
    agxVec128 savedQ5;
    agxVec128 savedQ6;
    agxVec128 savedQ7;
    agxVec128 savedQ8;
    agxVec128 savedQ9;
    agxVec128 savedQ10;
    agxVec128 savedQ11;
    agxVec128 savedQ12;
    agxVec128 savedQ13;
    agxVec128 savedQ14;
    agxVec128 savedQ15;
    agxVec128 savedQ16;
    agxVec128 savedQ17;
    agxVec128 savedQ18;
    agxVec128 savedQ19;
    agxVec128 savedQ20;
    agxVec128 savedQ21;
    agxVec128 savedQ22;
    agxVec128 savedQ23;
    agxVec128 savedQ24;
    agxVec128 savedQ25;
    agxVec128 savedQ26;
    agxVec128 savedQ27;
    agxVec128 savedQ28;
    agxVec128 savedQ29;
    agxVec128 savedQ30;
    agxVec128 savedQ31;
    u32 savedFpsr;
    u32 savedFpcr;
} sptmPerCpuState;

typedef struct sptmDispatchTableEntry {
    u64 dispatchEntryPoint;
    u64 permissions;
} sptmDispatchTableEntry;

typedef struct sptmStateTransition {
    u8 nextState;
    u8 padA[7];
    u64 actionMarker;
    u8 actionFlags;
    u8 padB[7];
    u64 actionBits;
} sptmStateTransition;

enum sptmDispatchLayout {
    sptmDomainTableStride = 384,
    sptmTableEntryStride = 24,
    sptmStateRowStride = 480,
    sptmTransitionEntryStride = 32,
    sptmMaxEventType = 15,
    sptmMaxState = 23,
    sptmMutableDomainLow = 1,
    sptmMutableDomainHigh = 3
};

#define sptmDispatchEntryAddr 0xfffffff02708053cULL
#define sptmGenterDispatchEntryAddr 0xfffffff0270c0370ULL
#define sptmDispatchStateMachineAddr 0xfffffff0270bfbc0ULL
#define sptmRegisterDispatchTableAddr 0xfffffff0270bf760ULL
#define sptmStateTransitionTableAddr 0xfffffff027015bb0ULL
#define sptmFixedDomainTableAddr 0xfffffff027078de0ULL
#define sptmMutableDomainTableAddr 0xfffffff027079560ULL
#define sptmDomainNameArrayAddr 0xfffffff027018950ULL
#define sptmSelfDispatchHandlerAddr 0xfffffff0270c01e0ULL

#define sptmVecCurrentSp0SyncFaultAddr 0xfffffff0270826c4ULL
#define sptmVecCurrentSp1SyncFaultAddr 0xfffffff027082744ULL
#define sptmFaultReportEntryAddr 0xfffffff0270c04d8ULL
#define sptmFaultClassifierAddr 0xfffffff0270c0aecULL
#define sptmGl1VectorTableAddr 0xfffffff027084000ULL

#define sptmColdBootEntryAddr 0xfffffff027088388ULL
#define sptmColdBootEntryEnd 0xfffffff02708b7fcULL
#define sptmSecondaryBootEntryAddr 0xfffffff02708b400ULL
#define sptmSecondaryBootSharedAddr 0xfffffff02708b40cULL
#define sptmInitEntryAddr 0xfffffff027097dd8ULL
#define sptmGxfSetupAddr 0xfffffff02708b8e8ULL
#define sptmGxfTestTargetAddr 0xfffffff02708b88cULL
#define sptmLaunchTxmAddr 0xfffffff0270810b8ULL
#define sptmTxmHandoffArgAddr 0xfffffff0270dd380ULL
#define sptmPabEntryTargetAddr 0xfffffff027096e98ULL

AGX_STATIC_ASSERT(sizeof(agxVec128) == 16);
AGX_STATIC_ASSERT(offsetof(sptmPerCpuState, savedX0) == 0x450);
AGX_STATIC_ASSERT(offsetof(sptmPerCpuState, savedQ0) % 16 == 0);

#endif
