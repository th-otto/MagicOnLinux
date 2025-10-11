#include <atomic>

// global variables
extern std::atomic_bool gbAtariVideoBufChanged;
extern uint8_t *addrOpcodeROM;
extern uint32_t addr68kVideo;			// start of 68k video memory (68k address)
extern uint32_t addr68kVideoEnd;		// end of 68k video memory (68k address)
extern bool gbAtariVideoRamHostEndian;	// true: video RAM is stored in host endian-mode
#ifdef _DEBUG
extern uint32_t addrOsRomStart;			// beginning of write-protected memory area (68k address)
extern uint32_t addrOsRomEnd;			// end of write-protected memory area (68k address)
#endif
extern uint8_t *hostVideoAddr;			// start of host video memory (host address)
