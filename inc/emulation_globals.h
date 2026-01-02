#include <stdint.h>

#if defined(__cplusplus)
#include <atomic>
extern std::atomic_bool gbAtariVideoBufChanged;
extern bool gbAtariVideoRamHostEndian;  // true: video RAM is stored in host endian-mode
#endif

// global variables
extern uint8_t *mem68k;                 // host pointer to memory block used by emulator
extern uint32_t mem68kSize;             // complete address range for 68k emulator, but without video memory
extern uint32_t memVideo68kSize;        // size of emulated video memory, including padding, i.e. 32768 instead of 32000
extern uint32_t memVideo68kSizeVisible; // size of emulated video memory, without padding, visible part
extern uint8_t *addrOpcodeROM;
extern uint32_t addr68kVideo;           // start of 68k video memory (68k address)
extern uint32_t addr68kVideoEnd;        // end of 68k video memory (68k address)
extern uint32_t addrOsRomStart;         // beginning of optionally write-protected memory area (68k address)
extern uint32_t addrOsRomEnd;           // end of write-protected memory area (68k address)

extern uint8_t *hostVideoAddr;          // start of host video memory (host address)
extern volatile unsigned char sExitImmediately;     // m68kcpu.c

// global functions
#if defined(__cplusplus)
extern "C" {
#endif
const char *AtariAddr2Description(uint32_t addr);
extern int m68k_get_super();
extern const char *exception68k_to_name(uint32_t addr);
#if defined(__cplusplus)
}
#endif

/*
* debugging stuff
*/

#ifndef NDEBUG
//#define M68K_BREAKPOINTS   4            // for debugging 68k code
//#define M68K_WRITE_WATCHES 3            // for debugging 68k code
// #define M68K_TRACE  256

#if defined(M68K_TRACE)
extern uint32_t m68k_trace[M68K_TRACE][3];
#if defined(__cplusplus)
extern "C" {
#endif
extern void m68k_trace_print();
#if defined(__cplusplus)
}
#endif
#endif

#if defined(M68K_BREAKPOINTS)
extern uint32_t m68k_breakpoints[M68K_BREAKPOINTS][2];      //  68k address and range, usually 0
#endif

#if defined(M68K_WRITE_WATCHES)
extern uint32_t m68k_write_watches[M68K_WRITE_WATCHES];
#endif

extern int do_not_interrupt_68k;        // for debugging

#if defined(__cplusplus)
extern "C" {
#endif
extern void int68k_enable(int enable); // disable interrupts, essential for debugging
#if defined(__cplusplus)
}
#endif
#endif      // NDEBUG
