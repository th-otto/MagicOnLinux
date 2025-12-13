;Trapnummern
GEMDOS				EQU 1
BIOS					EQU 13
XBIOS 				EQU 14

;Vektornummern
VBLVEC				EQU 28
GEMDOSVEC			EQU 33
BIOSVEC				EQU 45
XBIOSVEC 			EQU 46

;Trapadressen
GEMDOSVEC_ADDR 	EQU $84
BIOSVEC_ADDR		EQU $b4
XBIOSVEC_ADDR		EQU $b8

;GEMDOS-Funktiosnummern
CCONWS				EQU 9
DSETDRV				EQU 14
SCRNMALLOC			EQU 21
DGETDRV				EQU 25
FSETDTA				EQU 26
TGETDATE 			EQU 42
FGETDTA				EQU 47
PTERMRES 			EQU 49
DSETPATH 			EQU 59
FCREATE				EQU 60
FOPEN 				EQU 61
FCLOSE				EQU 62
FREAD 				EQU 63
FWRITE				EQU 64
FSEEK 				EQU 66
MXALLOC				EQU 68
DGETPATH 			EQU 71
MALLOC				EQU 72
MFREE 				EQU 73
MSHRINK				EQU 74
PEXEC 				EQU 75
FSFIRST				EQU 78
FSNEXT				EQU 79

;Bios-Funktionsnummern
BCONSTAT 			EQU 1
BCONIN				EQU 2
BCONOUT				EQU 3
SETEXC				EQU 5
TICKCAL				EQU 6
KBSHIFT				EQU 11

;Device Console
CON					EQU 2

;Xbios-Funktiosnummern
PHYSBASE				EQU	2
LOGBASE				EQU	3
GETREZ				EQU 4
SETSCREEN			EQU 5
SETPALETTE 			EQU 6
SETCOLOR 			EQU 7
IOREC 				EQU 14
SCRDMP				EQU 20
CURSCONF 			EQU 21

DOSOUND				EQU 32
KBDVBASE 			EQU 34
SUPEXEC				EQU 38
BLITMODE 			EQU 64
ESETSHIFT			EQU 80
ESETBANK 			EQU 82
ESETCOLOR			EQU 83
ESETPALETTE 		EQU 84
ESETGRAY 			EQU 86
ESETSMEAR			EQU 87
VSETMODE 			EQU 88
MON_TYPE 			EQU 89
VSETSYNC 			EQU 90
VGETSIZE 			EQU 91
VSETRGB				EQU 93
VGETRGB				EQU 94
VSETMASK 			EQU 150

CURS_GETRATE		EQU 5

ST_RAM_only 		EQU $00						;nur ST-RAM
TT_RAM_only 		EQU $01						;nur Alternative-RAM
ST_RAM_pref 		EQU $02						;ST-RAM bevorzugt
TT_RAM_pref 		EQU $03						;Alternative-RAM bevorzugt

PRIVATE_MEM 		EQU $10						;nur fÅr Prozeû und OS beschreibbar
GLOBAL_MEM			EQU $20						;fÅr alle zugÑnglich
SUPER_MEM			EQU $30						;fÅr alle dem Supervisor-Mode zugÑnglich
PRIVATER_MEM		EQU $40						;kann von allen gelesen werden, ansonsten wei PRIVATE

DONT_FREE			EQU $4000					;Bit 14 gesetzt

EINVFN				EQU -32						;Funktion nicht vorhanden

;Systemvariablen
etv_timer			EQU $0400					;System-Timer
resvalid 			EQU $0426					;RESET-Wert
resvector			EQU $042a					;RESET-Vektor
timer_ms 			EQU $0442					;Timer-Intervall
sshiftmd 			EQU $044c					;Auflîsung wie bei Getrez
v_bas_ad 			EQU $044e					;Zeiger auf den Bildspeicher
vblsem				EQU $0452					;Semaphore fÅr VBL
nvbls 				EQU $0454					;LÑnge der VBL-Queue
vbl_queue			EQU $0456					;Zeiger auf die Tabelle der VBlank-Prozesse
frclock				EQU $0466					;Anzahl der VBlanks
conterm				EQU $0484					;CON-Attributes
con_state			EQU $04a8					;Sprungvektor fÅr VT52
hz_200				EQU $04ba					;200Hz-Interrupt
dumpflag 			EQU $04ee					;Flag fÅr die Druckerausgabe
sysbase				EQU $04f2					;Zeiger auf Betriebssystemheader
dump_vec 			EQU $0502					;Zeiger auf Hardcopy-Routine
o_con 				EQU $0586					;Sprungvektor fÅr Bconout Åber CON
o_rawcon 			EQU $0592					;Sprungvektor fÅr Bconout Åber RAWCON
longframe			EQU $059e					;Flag fÅrs Stackframe
p_cookies			EQU $05a0					;Zeiger auf den Cookie-Jar
bell_hook			EQU $05ac					;Routine fÅr Glocke

;Systemheader-Offsets
os_version			EQU $02						;TOS-Version
os_beg				EQU $08						;Zeiger auf den Betriebssytemstart
os_magic 			EQU $14						;Zeiger auf die AES-Variablen
os_conf				EQU $1c						;Konfigurationsbits
kbshift				EQU $24						;Zeiger auf Tastenstatus (relativ zu sysbase)
run					EQU $28						;Zeiger auf die Basepage des aktuellen Prozesses

;Programmheader
ph_tlen				EQU 2 						;LÑnge des TEXT-segments
ph_dlen				EQU 6 						;LÑnge des DATA-segments
ph_blen				EQU 10						;LÑnge des BSS-Segments
ph_slen				EQU 14						;LÑnge der Symboltabelle
PH_LEN				EQU 28						;LÑnge des Programmheaders
PH_MAGIC 			EQU $601a					;Magic des Programmheaders

;Basepage-Offsets
p_lowtap 			EQU $00
p_hitpa				EQU $04
p_tbase				EQU $08
p_tlen				EQU $0c
p_dbase				EQU $10
p_dlen				EQU $14
p_bbase				EQU $18
p_blen				EQU $1c
p_dta 				EQU $20
p_parent 			EQU $24
p_env 				EQU $2c

;Defines fÅr den _VDO-Cookie
ST_VIDEO 			EQU 0
STE_VIDEO			EQU 1
TT_VIDEO 			EQU 2
FALCON_VIDEO		EQU 3

E_OK					EQU 0

;XBIOS-Video-Modi
ST_LOW				EQU 0
ST_MID				EQU 1
ST_HIGH				EQU 2
FALCONMDS			EQU 3
TT_MID				EQU 4
TT_HIGH				EQU 6
TT_LOW				EQU 7

;Defines fÅr Falcon-modecode
VERTFLAG 			EQU $0100
STMODES				EQU $80
OVERSCAN 			EQU $40
PAL					EQU $20
VGA					EQU $10
TV 					EQU $00

COL80 				EQU $08
COL40 				EQU $00
NUMCOLS				EQU $07
BPS16 				EQU $04
BPS8					EQU $03
BPS4					EQU $02
BPS2					EQU $01
BPS1					EQU $00

VTF_BIT				EQU 8
STC_BIT				EQU 7
OVS_BIT				EQU 6
PAL_BIT				EQU 5
VGA_BIT				EQU 4
CLM_BIT				EQU 3

;Prozessornummern
M68000				EQU 0
M68010				EQU 10
M68020				EQU 20
M68030				EQU 30
M68040				EQU 40

;Werte fÅr malloc_info
MALLOC_dontfree	EQU	0
MALLOC_ttram		EQU	1
MALLOC_malloc		EQU	2
MALLOC_act_pd		EQU	3
