/* Minimal arm-tok.h */

#ifdef TCC_TARGET_ARM64
/* ARM64 specific tokens */
#endif

#if defined(TCC_TARGET_ARM) || defined(TCC_TARGET_ARM64)
/* These are used for #pragma pack(push) / #pragma pack(pop) in tccpp.c
   even if the target doesn't use them for its assembler.
   tcctok.h includes this file inside the assembler token section.
*/
DEF(TOK_ASM_push, "push")
DEF(TOK_ASM_pop, "pop")
#endif
