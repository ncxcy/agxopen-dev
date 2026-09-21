#ifndef AGX_SPTMBOOTREGIONS_H
#define AGX_SPTMBOOTREGIONS_H

#include "agx/types.h"
typedef struct sptmBootRegion {
    const char *name;
    int typeCode;
} sptmBootRegion;

static const sptmBootRegion sptmBootRegionTable[] = {
    { "DeviceTree", 11 },
    { "SPTM-ro", 2 },
    { "SPTM-rx", 4 },
    { "SPTM-rw", 2 },
    { "SPTM-le", 2 },
    { "TXM-ro", 40 },
    { "TXM-rx", 40 },
    { "TXM-bx", 5 },
    { "TXM-rw", 40 },
    { "TXM-le", 40 },
    { "BootKC-ro", 11 },
    { "BootKC-rs", 2 },
    { "BootKC-rx", 11 },
    { "BootKC-rw", 11 },
    { "BootKC-le", 11 },
    { "AuxKC-rw", 11 },
    { "AuxKC-ro", 11 },
    { "AuxKC-rx", 11 },
    { "AuxKC-le", 11 },
    { "TrustCache", 0 },
    { "CL4-rx", 59 },
    { "CL4-ro", 59 },
    { "CL4-rw", 59 },
    { "CL4-le", 59 },
    { "CL4-dummypage", 59 },
    { "RAMDisk", 11 },
    { "RTBuddySeg", 11 },
    { "SEPFW", 11 },
    { "SEPPatches", 11 },
    { "uStuff", 11 },
    { "preoslog", 11 },
    { "SPTMDebug", 11 },
    { "BootArgs", 11 },
    { "ExclaveOSIntegrityCatalog", 59 },
    { "ExclaveOSTrustCache", 59 }
};

#define sptmBootRegionTableCount 35

AGX_STATIC_ASSERT(sizeof(sptmBootRegionTable) / sizeof(sptmBootRegionTable[0]) == sptmBootRegionTableCount);

#endif
