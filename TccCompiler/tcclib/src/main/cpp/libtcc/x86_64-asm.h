/* Minimal x86_64-asm.h */
#ifdef TCC_TARGET_X86_64
    DEF_ASM_OP0L(endbr64, 0xf30f1e, 7, OPC_MODRM)
    DEF_ASM_OP0(lfence, 0x0faee8)
    DEF_ASM_OP0(sfence, 0x0faef8)
    DEF_ASM_OP0(mfence, 0x0faef0)
#endif
