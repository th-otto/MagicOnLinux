;VDI-Funktionsnummern
V_SET_APP_BUF		EQU	-1
V_OPNWK				EQU	1
V_CLSWK				EQU	2
V_CLRWK				EQU	3
V_UPDWK				EQU	4
V_ESCAPE				equ	5
V_PLINE				EQU	6
V_GDP					EQU	11
VST_HEIGHT			EQU	12
VS_COLOR				EQU	14
VSL_TYPE				EQU	15
VSL_WIDTH			EQU	16
VSL_COLOR			EQU	17
VSM_TYPE				EQU	18
VSM_HEIGHT			EQU	19
VSM_COLOR			EQU	20
VST_FONT				EQU	21
VST_COLOR			EQU	22
VSF_INTERIOR		EQU	23
VSF_STYLE			EQU	24
VSF_COLOR			EQU	25
VQ_COLOR				EQU	26
V_LOCATOR			EQU	28
VQL_ATTRIBUTES		EQU	35
VQF_ATTRIBUTES		EQU	37
VST_ALIGNMENT		EQU	39
V_OPNVWK				EQU	100
V_CLSVWK				EQU	101
VQ_EXTND				EQU	102
V_CONTOURFILL		EQU	103
VSF_PERIMETER		EQU	104
V_GET_PIXEL			EQU	105
VSL_ENDS				EQU	108
VRO_CPYFM			EQU	109
VR_TRNFM				EQU	110
VSC_FORM				EQU	111
VSF_UDPAT			EQU	112
VSL_UDSTY			EQU	113
VR_RECFL				EQU	114
VST_LOAD_FONTS		EQU	119
VST_UNLOAD_FONTS	EQU	120
VRT_CPYFM			EQU	121
V_SHOW_C				EQU	122
V_HIDE_C				EQU	123
VQ_MOUSE				EQU	124
VEX_BUTV				EQU	125
VS_CLIP				EQU 	129
VQT_FONTINFO		EQU	131
VQT_JUSTIFIED		EQU	132
VQT_CACHESIZE		EQU	255

;VDI-Unterfunktionsnummern
;GDP
V_BAR					EQU	1
V_ELLIPSE			EQU	5
V_ELLPIE				EQU	7
V_JUSTIFIED			EQU	10
V_BEZ_ONOFF			EQU	13

;Escapes
VQ_CHCELLS			EQU	1
V_CURTEXT			EQU	12
VQ_CURADDRESS		EQU	15
V_RMCUR				EQU	19
V_FORM_ADV			EQU	20
V_OUTPUT_WINDOW	EQU	21
V_CLEAR_DISP_LIST	EQU	22
V_BIT_IMAGE			EQU	23
VQ_SCAN				EQU	24
V_ALPHA_TEXT		EQU	25
V_ORIENT				EQU	27
V_PS_HALFTONE		EQU	32
VQ_TRAY_NAMES		EQU	36
V_PAGE_SIZE			EQU	37
VQ_PAGE_NAME		EQU	38
V_SOUND				EQU	61
VS_MUTE				EQU	62
VS_CALIBRATE		EQU	76
VQ_CALIBRATE		EQU	77
V_META_EXTENTS		EQU	98
V_BEZ_QUAL			EQU	99
VM_PAGESIZE			EQU	99
VM_COORDS			EQU	99
VM_FILENAME			EQU	100
V_OFFSET				EQU	101
V_FONTINIT			EQU	102
V_ESCAPE2000		EQU	2000

;Fontheader
font_id				EQU	0						;Fontnummer
point					EQU	2						;Hîhe	in	Punkten
name					EQU	4						;Name	des	Zeichensatzes
first_ade			EQU	36						;erstes	Zeichen
last_ade				EQU	38						;letztes	Zeichen
top					EQU	40						;Abstand	Topline-> Baseline
ascent				EQU	42						;Abstand	Ascentline-> Baseline
half					EQU	44						;Abstand	Halfline-> Baseline
descent				EQU	46						;Abstand	Descentline-> Baseline
bottom				EQU	48						;Abstand Bottomline-> Baseline
max_char_width		EQU	50						;grîûte Zeichenbreite
max_cell_width		EQU	52						;grîûte Zellenbreite
left_offset			EQU	54						;Linker Rand fÅr Kursivschrift
right_offset		EQU	56						;Rechter Rand fÅr Kursivschrift
thicken				EQU	58						;Verbreiterungsfaktor fÅr Fettschrift
ul_size				EQU	60						;Dicke der Unterstreichung
;lighten				EQU	62						;Maske fÅr Helle Schrift
;skew					EQU	64						;Maske fÅr Kursivschrift
flags					EQU	66						;Flags fÅr Beschaffenheit des Fonts
;hor_table			EQU	68						;Zeiger auf Horizontal offset table
off_table			EQU	72						;Zeiger auf Character offset table
dat_table			EQU	76						;Zeiger auf Fontimage
form_width			EQU	80						;Breite des Font-Images in Bytes
form_height			EQU	82						;Hîhe des Font-Images in Zeilen
next_font			EQU	84						;Zeiger auf den nÑchsten Font

sizeof_FONT_HDR	EQU	88

;Bitnummern in flags
T_DEF_FONT_BIT		EQU	0						;Bitnummer fÅr Defaultfont
T_HOT_BIT			EQU	1						;Bitnummer der HOT
T_SWAP_BIT			EQU	2						;Bitnummer des Bytetausch-Flags
T_MONO_BIT			EQU	3						;Bitnummer fÅr Ñquidistante Fonts

;MFDB
fd_addr				EQU	0						;Zeiger auf Speicherblock
fd_w					EQU	4						;Breite in Punkten
fd_h					EQU	6						;Hîhe in Punktzeilen
fd_wdwidth			EQU	8						;Breite in Worten
fd_stand				EQU	10						;Format
fd_nplanes			EQU	12						;Anzahl der Bildebenen
fd_r1					EQU	14						;reserviert
fd_r2					EQU	16						;reserviert
fd_r3					EQU	18						;reserviert
MFDB_SIZE			EQU	20

sizeof_MFDB			EQU	20

;Offsets innerhalb von pblock
pb_contrl			EQU	0
pb_intin				EQU	4
pb_ptsin				EQU	8
pb_intout			EQU	12
pb_ptsout			EQU	16

sizeof_pb			EQU	20

;Offset in contrl
n_ptsin				EQU	2						;contrl[1]
n_ptsout				EQU	4						;contrl[2]
n_intin				EQU	6						;contrl[3]
n_intout 			EQU	8						;contrl[4]
opcode2				EQU	10						;contrl[5]
handle				EQU	12						;contrl[6]
s_addr				EQU	14						;contrl[7/8]
d_addr				EQU	18						;contrl[9/10]
f_addr				EQU	20						;contrl[10/11]

sizeof_contrl		EQU	32

;Linien
L_WIDTH_MIN			EQU	1						;minimale Linienbreite
L_WIDTH_MAX			EQU	99						;maximale Linienbreite
L_SOLID				EQU	1						;durchgehendes Linienmuster
L_USER_DEF			EQU	7						;benutzerdefiniertes Linienmuster
L_SQUARED			EQU	0						;eckiges Linienende
L_ARROW				EQU	1						;pfeilfîrmiges Linienende
L_ROUNDED			EQU	2						;rundes Linienende

;VerknÅpfungsmodi
REPLACE				EQU	1
TRANSPARENT			EQU	2
EX_OR					EQU	3
REV_TRANS			EQU	4

;Farben
WHITE					EQU	0						;Weiû
BLACK					EQU	1						;Schwarz

;Marker
M_DOT					EQU	1						;Punkt
M_ASTERISK			EQU	3						;Stern
M_DIAMOND			EQU	6						;Raute
M_WIDTH_MIN			EQU	1						;minimale Markerbreite
M_HEIGHT_MIN		EQU	1						;minimale Markerhîhe
M_WIDTH_MAX			EQU	1333					;maximale Markerbreite
M_HEIGHT_MAX		EQU	999					;maximale Markerhîhe

MARKER_COUNT		EQU	0
MARKER_ADDWIDTH	EQU	2
MARKER_OFFX			EQU	4
MARKER_OFFY			EQU	6

;Text
T_SYSTEM_FACE		EQU	1						;Systemzeichensatz

T_LEFT_ALIGN		EQU	0						;linksbÅndige Ausrichtung
T_MID_ALIGN			EQU	1						;zentrierte Ausrichtung
T_RIGHT_ALIGN		EQU	2						;rechtsbÅndige Ausrichtung

T_BASE_ALIGN		EQU 0 						;Zeichenbasislinie
T_HALF_ALIGN		EQU 1 						;Zeichenhalblinie
T_ASCENT_ALIGN 	EQU 2 						;Zeichenoberkante
T_BOTTOM_ALIGN 	EQU 3 						;Zellenunterkante
T_DESCENT_ALIGN	EQU 4 						;Zeichenunterkante
T_TOP_ALIGN 		EQU 5 						;Zellenoberkante

T_BOLD				EQU 1 						;Maske fÅr fett
T_LIGHT				EQU 2 						;Maske fÅr hell
T_ITALICS			EQU 4 						;Maske fÅr kursiv
T_UNDERLINED		EQU 8 						;Maske fÅr unterstrichen
T_OUTLINED			EQU 16						;Maske fÅr umrandet
T_BOLD_BIT			EQU 0 						;Bitnummer fÅr fett
T_LIGHT_BIT 		EQU 1 						;Bitnummer fÅr hell
T_ITALICS_BIT		EQU 2 						;Bitnummer fÅr kursiv
T_UNDERLINED_BIT	EQU 3 						;Bitnummer fÅr unterstrichen
T_OUTLINED_BIT 	EQU 4 						;Bitnummer fÅr umrandet

T_ROT_0 EQU 0
T_ROT_90 EQU 1
T_ROT_180 EQU 2
T_ROT_270 EQU 3

;FÅllmuster
F_HOLLOW 			EQU 0 						;hohl
F_SOLID				EQU 1 						;durchgehend
F_PATTERN			EQU 2 						;gemustert (Graustufen)
F_HATCH				EQU 3 						;schraffiert
F_USER_DEF			EQU 4 						;benutzerdefiniert

;EingabegerÑte
I_MOUSE				EQU 1 						;Maus
I_CURSOR 			EQU 2 						;Cursortasten
I_FUNCTION_KEY 	EQU 3 						;Funktionstasten
I_KEYBOARD			EQU 4 						;Tastatur allgemein
I_REQUEST			EQU 1 						;Request-Eingabemodus
I_SAMPLE 			EQU 2 						;Sample-Eingabemodus

;Linie
SQUARE				EQU 0
ARROW 				EQU 1
ROUND 				EQU 2

;Treibernummen in driver_id (von 0 aus gezÑhlt)
SCREEN				EQU 0 						;Bildschirm-Treiber
SCREEN0				EQU 0
SCREEN9				EQU 9
PLOTTER				EQU 10						;Plotter-Treiber
PRINTER				EQU 20						;Drucker-Treiber
METAFILE 			EQU 30						;Metafile-Treiber
CAMERA				EQU 40						;Kamera-Treiber
TABLETT				EQU 50						;Grafiktablett-Treiber
MEMORY				EQU 60						;Bitmap-Treiber


.OFFSET
										;typedef struct
										;{
XFI_size:			DS.L	1		;	LONG	size;
XFI_format:			DS.W	1		;	WORD	format;
XFI_id:				DS.W	1		;	UWORD	id;
XFI_index:			DS.W	1		;	WORD	index;
XFI_font_name:		DS.B	50		;	BYTE	font_name[50];
XFI_family_name:	DS.B	50		;	BYTE	family_name[50];
XFI_style_name:	DS.B	50		;	BYTE	style_name[50];
XFI_file_name1:	DS.B	200	;	BYTE	file_name1[200];
XFI_file_name2:	DS.B	200	;	BYTE	file_name2[200];
XFI_file_name3:	DS.B	200	;	BYTE	file_name3[200];
XFI_pt_cnt:			DS.W	1		;	WORD	pt_cnt;
XFI_pt_sizes:		DS.W	64		;	WORD	pt_sizes[64];
										;} XFNT_INFO;

sizeof_XFNT_INFO: