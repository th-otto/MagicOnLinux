PTRLEN	EQU	4		; Zeiger auf Elementfunktion braucht 4 Zeiger

	OFFSET

;Atari -> Host
MacSysX_magic:		DS.L 1		; $00 ist 'MagC'
MacSysX_len:		DS.L	1		; $04 LÑnge der Struktur
MacSysX_syshdr:	DS.L 1		; $08 Adresse des Atari-Syshdr
MacSysX_keytabs:	DS.L 1		; $0c 5*128 Bytes fÅr Tastaturtabellen
MacSysX_mem_root:	DS.L 1		; $10 Speicherlisten
MacSysX_act_pd:	DS.L 1		; $14 Zeiger auf aktuellen Prozeû
MacSysX_act_appl:	DS.L 1		; $18 Zeiger auf aktuelle Task
MacSysX_verAtari:	DS.L 1		; $1c Versionsnummer MagicMacX.OS
;Host -> Atari
MacSysX_verMac:	DS.L	1		; $20 Versionsnummer der Struktur
MacSysX_cpu:		DS.W 1		; $24 CPU (30=68030, 40=68040)
MacSysX_fpu:		DS.W 1		; $26 FPU (0=nix,4=68881,6=68882,8=68040)
MacSysX_init:		DS.L 1		; $28 Wird beim Warmstart des Atari aufgerufen
MacSysX_dev_in:	DS.L 1		; $2c Bconin()
MacSysX_dev_out:	DS.L 1		; $30 Bconout()
MacSysX_Ikbdws:	DS.L 1		; $34 Ikbdws()
MacSysX_biosinit:	DS.L 1		; $38 nach Initialisierung aufrufen
MacSysX_dev_istat:	DS.L 1		; $3c Bconstat()
MacSysX_dev_ostat:	DS.L 1		; $40 Bcostat()
MacSysX_Dosound:	DS.L 1		; $44 Dosound()
MacSysX_VdiInit:	DS.L PTRLEN	; $48 nach Initialisierung des VDI aufrufen
MacSysX_pixmap:	DS.L 1		; $58 Daten fÅrs VDI
MacSysX_pMMXCookie:	DS.L	1		; $5c 68k-Zeiger auf MgMx-Cookie
MacSysX_Xcmd:		DS.L PTRLEN	; $60 XCMD-Kommandos
MacSysX_PPCAddr:	DS.L 1		; $70 tats. PPC-Adresse von 68k-Adresse 0
MacSysX_VideoAddr:	DS.L	1		; $74 tats. PPC-Adresse des Bildschirmspeichers
MacSysX_Exec68k:	DS.L	PTRLEN	; $78 hier kann der PPC-Callback 68k-Code ausfÅhren
MacSysX_gettime:	DS.L 1		; $88 LONG GetTime(void) Datum und Uhrzeit ermitteln
MacSysX_settime:	DS.L	1		; $8c void SetTime(LONG *time) Datum/Zeit setzen
MacSysX_Setpalette:	DS.L	1		; $90 void Setpalette( int ptr[16] )
MacSysX_Setcolor:	DS.L	1		; $94 int Setcolor( int nr, int val )
MacSysX_VsetRGB:	DS.L	1		; $98 void VsetRGB( WORD index, WORD count, LONG *array )
MacSysX_VgetRGB:	DS.L	1		; $9c void VgetRGB( WORD index, WORD count, LONG *array )
MacSysX_syshalt:	DS.L 1		; $a0 SysHalt( char *str ) "System halted"
MacSysX_syserr:	DS.L 1		; $a4 SysErr( long val ) "a1 = 0 => Bomben"
MacSysX_coldboot:	DS.L 1		; $a8 ColdBoot(void) Kaltstart ausfÅhren
MacSysX_exit:	  	DS.L 1		; $ac Exit(void) beenden
MacSysX_debugout:	DS.L 1		; $b0 MacPuts( char *str ) fÅrs Debugging
MacSysX_error: 	DS.L 1		; $b4 d0 = -1: kein Grafiktreiber
MacSysX_resb8: 	DS.L 1		; $b8
MacSysX_resbc: 	DS.L 1		; $bc
MacSysX_resc0:		DS.L 1		; $c0
MacSysX_prn_wrts:	DS.L	1		; $c4 LONG PrnWrts({char *buf, LONG count}) String auf Drucker
MacSysX_serconf:	DS.L 1		; $c8 Rsconf( void *params ) fÅr ser1
MacSysX_sercc: 	DS.L 1		; $cc
MacSysX_resd0: 	DS.L 1		; $d0
MacSysX_resd4: 	DS.L 1		; $d4
MacSysX_resd8:		DS.L 1		; $d8
MacSysX_SerOpen:	DS.L 1		; DEV_SER: Serielle Schnittstelle îffnen
MacSysX_SerClose: 	DS.L 1		; DEV_SER: Serielle Schnittstelle schlieûen
MacSysX_SerRead: 	DS.L 1		; DEV_SER: Mehrere Zeichen von seriell lesen
MacSysX_SerWrite:	DS.L 1		; DEV_SER: Mehrere Zeichen auf seriell schreiben
MacSysX_SerStat:	DS.L 1		; DEV_SER: Lese-/Schreibstatus fÅr serielle Schnittstelle
MacSysX_SerIoctl:	DS.L 1		; DEV_SER: Ioctl-Aufrufe fÅr serielle Schnittstelle
MacSysX_GetKbOrMous: DS.L PTRLEN	; Liefert Tastatur/Maus
MacSysX_dos_macfn:	DS.L	1		; DosFn({int,void*} *) DOS-Funktionen 0x60..0xfe
MacSysX_xfs_version: DS.L 1		; Version des Mac-XFS
MacSysX_xfs_flags:	DS.L	1		; Flags fÅr das Mac-XFS
MacSysX_xfs:		DS.L PTRLEN	; zentrale Routine fÅr das XFS
MacSysX_xfs_dev:	DS.L PTRLEN	; zugehîriger Dateitreiber
MacSysX_drv2devcode: DS.L PTRLEN	; umrechnen Laufwerk->Devicenummer
MacSysX_rawdrvr:	DS.L PTRLEN	; LONG RawDrvr({int, long} *) Raw-Driver (Eject) fÅr Mac
MacSysX_Daemon:	DS.L 1		; Aufruf fÅr den mmx-Daemon
MacSysX_BlockDev:   DS.L 1         ; MagicOnLinux soll hier disk images verwalten
MacSysX_Network:    DS.L 1         ; Netzwerkfunktionen
MacSysX_Setscreen:  DS.L 1         ; XBIOS #5, void Setscreen(void *log, void *phys, int res) 
MacSysX_Yield:		DS.L 1		; Aufruf fÅr Rechenzeit abgeben
MacSys_OldHdr:		DS.L	49		; KompatibilitÑt mit Behnes
MacSysX_sizeof:

	TEXT

; Prozedur aufrufen. a0 auf Zeiger, a1 ist Parameter.

MACRO	MACPPC
		DC.W $00c0
		ENDM

; Elementfunktion aufrufen. a0 auf 4 Zeiger, a1 ist Parameter

MACRO	MACPPCE
		DC.W $00c1
		ENDM
