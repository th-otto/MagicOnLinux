;Line-A-Basisadressen
TOS_100_BASE		EQU $293a					;TOS	 1.00
TOS_102_BASE		EQU $299a					;TOS	 1.02
TOS_103_BASE		EQU $2170					;TOS	 1.03 vom 8.8.1988 (1.04 Beta)
TOS_104_BASE		EQU $2adc					;TOS	 1.04
TOS_162_BASE		EQU $2b1c					;TOS	 1.62
TOS_301_BASE		EQU $3930					;TOS	 3.01
TOS_306_BASE		EQU $3a1a					;TOS	 3.06
TOS_401_BASE		EQU $3e86					;TOS	 4.01
KAOS_14_BASE		EQU $2adc					;KAOS  1.4x
MAGIX_100_BASE 	EQU $2500					;MagiX 1.00
MAGIX_110_BASE 	EQU $2070					;MagiX 1.10

;CMP_BASE			 EQU TOS_306_BASE 		 ;Vergleichswert fÅr die Relokation
;LINE_A_BASE		 EQU TOS_306_BASE 		 ;Adresslage von LINE-A
;CMP_BASE			 EQU TOS_401_BASE 		 ;Vergleichswert fÅr die Relokation
;LINE_A_BASE		 EQU TOS_401_BASE 		 ;Adresslage von LINE-A
CMP_BASE 			EQU MAGIX_110_BASE		;Vergleichswert fÅr die Relokation
LINE_A_BASE 		EQU MAGIX_110_BASE		;Adresslage von LINE-A
;CMP_BASE		  EQU KAOS_14_BASE				;Vergleichswert fÅr die Relokation
;LINE_A_BASE		 EQU KAOS_14_BASE 		 ;Adresslage von LINE-A

;Zeiger auf den aktuellen Fonts
CUR_FONT 			EQU LINE_A_BASE-$038a	;aktueller Font
;Sprite-Definition-Block fÅr den Mauszeiger
M_POS_HX 			EQU LINE_A_BASE-$0358	;x-Hot-Spot
M_POS_HY 			EQU LINE_A_BASE-$0356	;y-Hot-Spot
M_PLANES 			EQU LINE_A_BASE-$0354	;Anzahl der Ebenen (immer 1)
M_CDB_BG 			EQU LINE_A_BASE-$0352	;Hintergrundfarbe
M_CDB_FG 			EQU LINE_A_BASE-$0250	;Vordergrundfarbe
MASK_FORM			EQU LINE_A_BASE-$034e	;Maskendaten
;Ausgabedaten von vq_extnd int intout
INQ_TAB				EQU LINE_A_BASE-$030e	;int INQ_TAB[45]
INQ_TAB0 			EQU INQ_TAB 				;Bildschirmtyp
INQ_TAB1 			EQU INQ_TAB+2				;Anzahl Farbabstufungen
INQ_TAB4 			EQU INQ_TAB+8				;Anzahl der Bildebenen
INQ_TAB5 			EQU INQ_TAB+10 			;CLUT-UnterstÅtzung
INQ_TAB6 			EQU INQ_TAB+12 			;Anzahl der 16*16 Rasteroperationen
INQ_TAB19			EQU INQ_TAB+38 			;Clipping-Flag
;Ausgabedaten on v_opnwk/v_opnvwk int intout
DEV_TAB				EQU LINE_A_BASE-$02b4	;int DEV_TAB[45]
DEV_TAB0 			EQU DEV_TAB 				;adressierbare Rasterbreite
DEV_TAB1 			EQU DEV_TAB+2				;adressierbare Rasterhîhe
DEV_TAB3 			EQU DEV_TAB+6				;Pixelbreite in Mikrometern
DEV_TAB4 			EQU DEV_TAB+8				;Pixelhîhe in Mikrometern
DEV_TAB5 			EQU DEV_TAB+10 			;Anzahl der Zeichenhîhen
DEV_TAB6 			EQU DEV_TAB+12 			;
DEV_TAB13			EQU DEV_TAB+26 			;Anzahl der Farben
DEV_TAB35			EQU DEV_TAB+70 			;Farbdarstellungsflag
DEV_TAB39			EQU DEV_TAB+78 			;Grîûe der Farbpalette
;Mausdaten
GCURX 				EQU LINE_A_BASE-$025a	;X-Koordinate
GCURY 				EQU LINE_A_BASE-$0258	;Y-Koordinate
M_HID_CT 			EQU LINE_A_BASE-$0256	;Hide-Counter des Mauszeigers
MOUSE_BT 			EQU LINE_A_BASE-$0254	;Mausknopfstatus
;Die ersten 16 bei vs_color eingestellten FarbintensitÑten
REQ_COL				EQU LINE_A_BASE-$0252	;int REQ_COL[3][16]
;Ausgabedaten von v_opnwk/v_opnvwk in ptsout
SIZ_TAB				EQU LINE_A_BASE-$01f2	;int SIZ_TAB[15]
SIZ_TAB2 			EQU SIZ_TAB+4				;grîûte Zeichenbreite
SIZ_TAB3 			EQU SIZ_TAB+6				;grîûte Zeichenhîhe
SIZ_TAB10			EQU SIZ_TAB+20 			;grîûte Markerbreite
SIZ_TAB11			EQU SIZ_TAB+22 			;grîûte Markerhîhe
;
RESERVED4			EQU LINE_A_BASE-$01d4	;RESERVED4[2]
;Adresse der aktuellen VDI-Workstation
CUR_WORK 			EQU LINE_A_BASE-$01d0	;Adresse der aktuellen Workstation
;Daten fÅr die Zeichensatzverwaltung
DEF_FONT 			EQU LINE_A_BASE-$01cc	;Adresse des Standardsystemfonts
FONT_RING			EQU LINE_A_BASE-$01c8	;FONT_HDR *FONT_RING[4]
FONT_RING2			EQU FONT_RING+8
FONT_COUNT			EQU LINE_A_BASE-$01b8	;Zeichensatzanzahl in FONT_RING
;
RESERVED3			EQU LINE_A_BASE-$01b6	;RESERVED3[45]
;Bewegungsflag und Status der Maustasten
CUR_MS_STAT 		EQU LINE_A_BASE-$015c
;Flag fÅr Sichtbarkeit des Textcursors
V_HID_CNT			EQU LINE_A_BASE-$015a
;Daten des Mauszeigers
CUR_X 				EQU LINE_A_BASE-$0158	;x-Koordinate des Mauszeigers
CUR_Y 				EQU LINE_A_BASE-$0156	;y-Koordinate des Mauszeigers
CUR_FLAG 			EQU LINE_A_BASE-$0154	;Flag fÅrs Zeichnen des Mauszeigers
MOUSE_FLAG			EQU LINE_A_BASE-$0153	;Flag fÅr den Mauszeiger (0 = ein)
;
RESERVED1			EQU LINE_A_BASE-$0152
;Bei ESC j gesicherte Koordinaten des Textcursors
V_SAV_XY 			EQU LINE_A_BASE-$014e
;Sprite-Save-Buffer fÅr maximal 4 Bildebenen
SAVE_LEN 			EQU LINE_A_BASE-$014a	;Zeilenanzahl
SAVE_ADDR			EQU LINE_A_BASE-$0148	;Hintergrundadresse
SAVE_STAT			EQU LINE_A_BASE-$0144	;Status des Hintergrundbuffers
SAVE_AREA			EQU LINE_A_BASE-$0142	;Hintergrundbuffer
;Verschiedene Interrupt-Vektoren des VDIs
USER_TIM 			EQU LINE_A_BASE-$42		;Vektor fÅr vex_timv
NEXT_TIM 			EQU LINE_A_BASE-$3e		;Vektor fÅr vex_timv
USER_BUT 			EQU LINE_A_BASE-$3a		;Vektor fÅr vex_butv
USER_CUR 			EQU LINE_A_BASE-$36		;Vektor fÅr vex_curv
USER_MOT 			EQU LINE_A_BASE-$32		;Vektor fÅr vex_motv
;Daten des VT52-Emulators
V_CEL_HT 			EQU LINE_A_BASE-$2e		;Zeichenhîhe in Zeilen
V_CEL_MX 			EQU LINE_A_BASE-$2c		;Textspaltenanzahl -1
V_CEL_MY 			EQU LINE_A_BASE-$2a		;Textzeilenanzahl -1
V_CEL_WR 			EQU LINE_A_BASE-$28		;TextzeilenlÑnge in Bytes
V_COL_BG 			EQU LINE_A_BASE-$26		;Texthintergrundfarbe
V_COL_FG 			EQU LINE_A_BASE-$24		;Textvordergrundfarbe
V_CUR_AD 			EQU LINE_A_BASE-$22		;aktuelle Cursoradresse
V_CUR_OF 			EQU LINE_A_BASE-$1e		;Byte-Offset zum Bildschirmanfang
V_CUR_XY 			EQU LINE_A_BASE-$1c		;V_CUR_XY[2]
V_CUR_XY0			EQU LINE_A_BASE-$1c		;x-Koordinate des Textcursors
V_CUR_XY1			EQU LINE_A_BASE-$1a		;y-Koordinate des Textcursors
V_PERIOD 			EQU LINE_A_BASE-$18		;Textcursor-Blinkfrequenz in 1/70
V_CUR_CT 			EQU LINE_A_BASE-$17		;ZÑhler fÅrs Textcursorblinken
V_FNT_AD 			EQU LINE_A_BASE-$16		;Adresse des Fontimage
V_FNT_ND 			EQU LINE_A_BASE-$12		;Nummer des ersten Zeichens
V_FNT_ST 			EQU LINE_A_BASE-$10		;Nummer des letzten Zeichens
V_FNT_WD 			EQU LINE_A_BASE-$0e		;Breite des Fontimage in Bytes
V_REZ_HZ 			EQU LINE_A_BASE-$0c		;horizontale Auflîsung in Pixeln
V_OFF_AD 			EQU LINE_A_BASE-$0a		;Zeiger auf die Offset-Tabelle
RESERVED 			EQU LINE_A_BASE-$06		;Status des Textcursors
V_REZ_VT 			EQU LINE_A_BASE-$04		;vertikale Auflîsung
;Rastergrunddaten
BYTES_LIN			EQU LINE_A_BASE-$02		;Bytes pro Bildzeile

PLANES				EQU LINE_A_BASE			;Anzahl der Bildebenen
WIDTH 				EQU LINE_A_BASE+$02		;Bytes pro Bildzeile
;PB von LINE-A
CONTRL				EQU LINE_A_BASE+$04		;*contrl
INTIN 				EQU LINE_A_BASE+$08		;*intin
PTSIN 				EQU LINE_A_BASE+$0c		;*ptsin



INTOUT				EQU LINE_A_BASE+$10		;*intout
PTSOUT				EQU LINE_A_BASE+$14		;*ptsout
;Flags fÅr die Bildebenenbenutzung
COLBIT0				EQU LINE_A_BASE+$18		;Flag fÅr Benutzung von Bildebene 0
COLBIT1				EQU LINE_A_BASE+$1a		;Flag fÅr Benutzung von Bildebene 1
COLBIT2				EQU LINE_A_BASE+$1c		;Flag fÅr Benutzung von Bildebene 2
COLBIT3				EQU LINE_A_BASE+$1e		;Flag fÅr Benutzung von Bildebene 3
;Daten fÅr Linien
LSTLIN				EQU LINE_A_BASE+$20		;Flag fÅrs Setzen des Linienendpunkts
LNMASK				EQU LINE_A_BASE+$22		;Linienmuster
;VerknÅpfungsmodus fÅr Grafikoperationen
WMODE 				EQU LINE_A_BASE+$24		;0 = REPLACE, 1 = TRANSPARENT,
WMODE_LOW			EQU LINE_A_BASE+$25		;2 = EOR 	, 4 = NOT TRANSPARENT
;Koordinaten fÅr Linien
X1 					EQU LINE_A_BASE+$26		;x1
Y1 					EQU LINE_A_BASE+$28		;y1
X2 					EQU LINE_A_BASE+$2a		;x2
Y2 					EQU LINE_A_BASE+$2c		;y2
;FÅllmusterdaten
PATPTR				EQU LINE_A_BASE+$2e		;FÅllmusteradresse
PATMSK				EQU LINE_A_BASE+$32		;Zeilenanzahl des FÅllmusters
MFILL 				EQU LINE_A_BASE+$34		;Flag fÅr farbiges FÅllmuster
;Daten fÅrs Clipping
CLIP					EQU LINE_A_BASE+$36		;Clipping-Flag
XMINCL				EQU LINE_A_BASE+$38		;clip_xmin
YMINCL				EQU LINE_A_BASE+$3a		;clip_ymin
XMAXCL				EQU LINE_A_BASE+$3c		;clip_xmax
YMAXCL				EQU LINE_A_BASE+$3e		;clip_ymax
;Variabeln fÅr TEXTBLT
XDDA					EQU LINE_A_BASE+$40		;0x8000 fÅr TEXTBLT (??)

DDAINC				EQU LINE_A_BASE+$42		;Vergrîûerungsfaktor
SCALDIR				EQU LINE_A_BASE+$44		;Vergrîûerungsrichtung
MONO					EQU LINE_A_BASE+$46		;Flag fÅr Ñquidistante Fonts
SOURCEX				EQU LINE_A_BASE+$48		;x-Quellkoordinate des Zeichens
SOURCEY				EQU LINE_A_BASE+$4a		;y-Quellkoordinate des Zeichens
DESTX 				EQU LINE_A_BASE+$4c		;x-Zielkoordinate

DESTY 				EQU LINE_A_BASE+$4e		;y-Zielkoordinate
DELX					EQU LINE_A_BASE+$50		;Breite des Zeichens
DELY					EQU LINE_A_BASE+$52		;Hîhe des Zeichens
FBASE 				EQU LINE_A_BASE+$54		;Adresse des Fontimages
FWIDTH				EQU LINE_A_BASE+$58		;Breite des Images in Bytes
STYLE 				EQU LINE_A_BASE+$5a		;Texteffekte
STYLELO				EQU LINE_A_BASE+$5b
LITEMASK 			EQU LINE_A_BASE+$5c		;Maske fÅr hellen Text
SKEWMASK 			EQU LINE_A_BASE+$5e		;Maske fÅr kursiven Text
WEIGHT				EQU LINE_A_BASE+$60		;Verbreiterung fÅr fetten Text
ROFF					EQU LINE_A_BASE+$62		;rechter Abstand fÅr Kursivschrift
LOFF					EQU LINE_A_BASE+$64		;linker Abstand fÅr Kursivschrift
SCALE 				EQU LINE_A_BASE+$66		;Flag fÅr Vergrîûerung
CHUP					EQU LINE_A_BASE+$68		;Rotationswinkel
TEXTFG				EQU LINE_A_BASE+$6a		;Vordergrundfarbe
SCRTCHP				EQU LINE_A_BASE+$6c		;Adresse des Texteffektbuffers
SCRPT2				EQU LINE_A_BASE+$70		;Abstand des zweiten Buffers
TEXTBG				EQU LINE_A_BASE+$72		;Hintergrundfarbe
;Flag fÅr die Unterscheidung von vro_cpyfm und vrt_cpyfm bei COPY RASTER
COPYTRAN 			EQU LINE_A_BASE+$74
;Zeiger auf eine Routine, die von SEEDFILL nach jeder Zeile aufgerufen wird
SEEDABORT			EQU LINE_A_BASE+$76
;erweiterte LINE-A-Adressen fÅr den TT
REQ_COL_TT			EQU LINE_A_BASE+$ae		;int [3][240] TT-PaletteneintrÑge
SSB_ADDR 			EQU LINE_A_BASE+$064e	;Zeiger auf den SSB
LINEA_COLOR 		EQU LINE_A_BASE+$0652	;Farbe beim LINE-A-Aufruf
;Flags fÅr die Bildebenenbenutzung
COLBIT4				EQU LINE_A_BASE+$0656	;Flag fÅr Benutzung von Bildebene 4
COLBIT5				EQU LINE_A_BASE+$0658	;Flag fÅr Benutzung von Bildebene 5
COLBIT6				EQU LINE_A_BASE+$065a	;Flag fÅr Benutzung von Bildebene 6
COLBIT7				EQU LINE_A_BASE+$065c	;Flag fÅr Benutzung von Bildebene 7
;Sprite-Save-Buffer fÅr maximal 8 Bildebenen
SAVE_LEN_TT 		EQU LINE_A_BASE+$065e	;Zeilenanzahl
SAVE_ADDR_TT		EQU LINE_A_BASE+$0660	;Hintergrundadresse
SAVE_STAT_TT		EQU LINE_A_BASE+$0664	;Status des Buffers
SAVE_AREA_TT		EQU LINE_A_BASE+$0666	;long [8][16] TT-Hintergrundbuffer

;BitBlt-Struktur
B_WD					EQU 0 						;Breite des Blocks
B_HT					EQU 2 						;Hîhe des Blocks
PLANE_CT 			EQU 4 						;Anzahl der Ebenen
FG_COL				EQU 6 						;Vordergrundfarbe
BG_COL				EQU 8 						;Hintergrundfarbe
OP_TAB				EQU 10						;VerknÅpfungsmodi
S_XMIN				EQU 14						;X-Koordinate des Quellrasters
S_YMIN				EQU 16						;Y-Koordinate des Quellrasters
S_FORM				EQU 18						;Zeiger aufs Quellraster
S_NXWD				EQU 22						;Bytes zum nÑchsten Wort der Ebene
S_NXLN				EQU 24						;Breite des Quellrasters in Bytes
S_NXPL				EQU 26						;Abstand zur nÑchsten Quellebene
D_XMIN				EQU 28						;X-Koordinate des Zielrasters
D_YMIN				EQU 30						;Y-Koordinate des Zielrasters
D_FORM				EQU 32						;Zeiger aufs Zielraster
D_NXWD				EQU 36						;Bytes zum nÑchsten Wort der Ebene
D_NXLN				EQU 38						;Breite des Zielrasters in Bytes
D_NXPL				EQU 40						;Abstand zur nÑchsten Zielebene
P_ADDR				EQU 42						;Zeiger aufs Muster oder 0
P_NXLN				EQU 46						;Abstand zur nÑchsten Musterzeile
P_NXPL				EQU 48						;Abstand zur nÑchsten Musterebene
P_MASK				EQU 50						;(MusterlÑnge -2) * Ebenenanzahl

;Bitnummern fÅrs Cursorflag RESERVED
CURSOR_BL			EQU 0 						;Blinken (aus/ein)
CURSOR_STATE		EQU 1 						;momentaner Status (unsichtbar/sichtbar)
CURSOR_ONOFF		EQU 2 						;Cursordarstellung (aus/ein)
WRAP					EQU 3 						;Wrapping (aus/ein)
INVERSE				EQU 4 						;inverse Darstellung
CURSOR_SAVED		EQU 5 						;Cursorposition
