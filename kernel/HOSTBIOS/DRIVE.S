**********************************************************************
***************     DMA- und Floppy- Bootroutinen   ******************
**********************************************************************
**********************************************************************

; Werden nach MagicOnLinux weitergereicht

**********************************************************************
*
* void dummy_hdv_init( void )
*

dummy_hdv_init:
 move.w	#1,-(sp)
 lea		(sp),a1					; a1 = Parameter
 lea		MSysX+MacSysX_BlockDev(pc),a0	; PPC-Adresse
 MACPPC							; Mac anspringen
 addq.l	#2,sp
 rts


**********************************************************************
*
* long dummy_rwabs(int flag, void *buf, int count, int recno, int dev)
*

dummy_rwabs:
 move.w	#2,-(sp)
 lea		(sp),a1					; a1 = Parameter
 lea		MSysX+MacSysX_BlockDev(pc),a0	; PPC-Adresse
 MACPPC							; Mac anspringen
 addq.l	#2,sp
 moveq    #EUNDEV,d0
 rts


**********************************************************************
*
* long dummy_getbpb(int drv)
*

dummy_getbpb:
 move.w	#3,-(sp)
 lea		(sp),a1					; a1 = Parameter
 lea		MSysX+MacSysX_BlockDev(pc),a0	; PPC-Adresse
 MACPPC							; Mac anspringen
 addq.l	#2,sp
 moveq    #0,d0		; NULL-Zeiger, d.h. BPB ist ungÅltig
 rts


**********************************************************************
*
* long dummy_mediach(int drive)
*

dummy_mediach:
 move.w	#4,-(sp)
 lea		(sp),a1					; a1 = Parameter
 lea		MSysX+MacSysX_BlockDev(pc),a0	; PPC-Adresse
 MACPPC							; Mac anspringen
 addq.l	#2,sp
 moveq    #EUNDEV,d0
 rts


**********************************************************************
*
* long dummy_boot()
*

dummy_boot:
 move.w	#5,-(sp)
 lea		(sp),a1					; a1 = Parameter
 lea		MSysX+MacSysX_BlockDev(pc),a0	; PPC-Adresse
 MACPPC							; Mac anspringen
 addq.l	#2,sp
 moveq	#1,d0
 rts
