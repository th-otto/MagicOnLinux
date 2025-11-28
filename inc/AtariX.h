/// @brief Static function host callback with two parameters
typedef uint32_t (*tfHostCallback)(uint32_t a1, unsigned char *emubase);
/// @brief CMagiC method host callback
typedef uint32_t (*tfHostCallbackCpp)(uint32_t a1, unsigned char *emubase);
/// @brief old PPC callback (no longer used)
// typedef uint32_t (*PPCCallback)(uint32_t params, uint8_t *AdrOffset68k);
union hostcall {
	tfHostCallback c;
	tfHostCallbackCpp cpp;
};
extern union hostcall jump_table[];
