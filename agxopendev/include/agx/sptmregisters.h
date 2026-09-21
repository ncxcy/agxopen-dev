#ifndef AGX_SPTMREGISTERS_H
#define AGX_SPTMREGISTERS_H
enum sptmSysRegEncodings {
    sysRegGxfStatusEl1 = 0,
    sysRegGxfConfigEl1 = 1,
    sysRegGxfEntryEl1 = 2,
    sysRegGxfPabEntryEl1 = 3,
    sysRegGxfConfigEl12 = 4,
    sysRegGxfEntryEl12 = 5,
    sysRegGxfPabEntryEl12 = 6,
    sysRegTpidrGl1 = 7,
    sysRegVbarGl1 = 8,
    sysRegSpsrGl1 = 9,
    sysRegAspsrGl1 = 10,
    sysRegEsrGl1 = 11,
    sysRegElrGl1 = 12,
    sysRegFarGl1 = 13,
    sysRegTpidrGl2 = 14,
    sysRegSpGl12 = 15,
    sysRegSprrConfigEl1 = 16,
    sysRegSprrPermEl0 = 17,
    sysRegSprrPermEl1 = 18
};

#endif
