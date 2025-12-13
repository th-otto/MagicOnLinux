;unverÑndert mÅssen die folgenden Adressen bleiben:
;device_id	10
;wr_mode		60
;colors		20
;l_color		70
;l_sdstyle	94
;t_color		100
;f_color		190
;f_style		194
;f_interior	192
;f_pointer	198

;Workstation-Attributtabelle
;;;;;;;;;;;;;;;;;;;;;;;;;;;;

WK_LENGTH			EQU	600					;LÑnge der WK ohne FÅllmuster

;Informationen fÅr die Treiberverwaltung
disp_addr1			EQU	0 						;Zeiger auf NVDI-Dispatcher
disp_addr2			EQU	4 						;Zeiger auf Treiber-Dispatcher
wk_handle			EQU	8 						;Handle des Treibers

;Koordinatensystem
driver_id			EQU	10						;.w GerÑte-ID
pixel_width 		EQU	12						;.w Breite eines Pixels in Mikrometern
pixel_height		EQU	14						;.w Hîhe eines Pixels in Mikrometern
res_x 				EQU	16						;.w Rasterbreite -1
res_y 				EQU	18						;.w Rasterhîhe -1
colors				EQU	20						;.w Anzahl der Farbstifte -1
res_ratio			EQU	22						;.w SeitenverhÑltnis <0: vertikal stauchen ; >0 vertikal dehnen

driver_type			EQU	24						;.b Treibertyp (NVDI oder GDOS)
driver_device		EQU	26						;.w GerÑt (Bildschirm-, Drucker-, Metafile oder z.B. Memory-Treiber)
;Treibertyp (GDOS oder NVDI) im hîchsten Bit von driver_device kodieren

;Treiberspezifische Daten

input_mode			EQU	30					;.b Modi der EingabegerÑte

;Notlîsungen:
;Der Allzweckbuffer, Notlîsung!
buffer_addr 		EQU	32						;.l Zeiger auf den Buffer
buffer_len			EQU	36						;.l LÑnge des Buffers in Bytes
bez_buffer			EQU	40						;.l Bezier-Buffer von v_set_app_buf
bez_buf_len 		EQU	44						;.l LÑnge des Bezier-Buffers

;Arrays fÅr Eingabekoordinaten
gdos_buffer 		EQU	48						;.l Buffer fÅr GDOS-Treiber

;Begrenzung der Grafikkommandos
clip_xmin			EQU	52						;Minimum - x
clip_ymin			EQU	54						;Minimum - y
clip_xmax			EQU	56						;Maximum - x
clip_ymax			EQU	58						;Maximum - y

;Logische VerknÅpfung der Grafikkommandos
wr_mode				EQU	60						;Grafikmodus

bez_on				EQU	62							;.w 0: Beziers aus 1: Beziers an
bez_qual 			EQU	64							;.w BezierqualitÑt von 0-5

;Liniendarstellung
l_color				EQU	70							;.w Linienfarbe
l_width				EQU	72							;.w Linienbreite
l_start				EQU	74							;.w Linienanfang
l_end 				EQU	76							;.w Linienende
l_lastpix			EQU	78							;.w 1 = letzten Punkt nicht setzen
l_style				EQU	80							;.w Linienstil
l_styles 			EQU	82							;.w Linienmuster[6]
l_sdstyle			EQU	94							;.w selbstdefinierter Linienstil

;Textdarstellung
t_color				EQU	100						;.w Textfarbe

t_mapping			EQU	105						;.b 0: direkt mit Index indizieren 1: t_asc_map benutzen
t_first_ade 		EQU	106						;.w Nummer des ersten Zeichens
t_ades				EQU	108						;.w Anzahl der Zeichen -1
t_space_index		EQU	110						;.w Index des Leerzeichens (Trennzeichen bei v_justified)
t_unknown_index	EQU	112						;.w Index fÅr ein nicht bekanntes Zeichen (wird bei falschem Index ausgegeben)
t_prop				EQU	114						;.b 1: Font ist proportional
t_grow				EQU	115						;.b -1: vergrîûern +1: verkleinern (nur Bitmap-Fonts) 
t_no_kern			EQU	116						;.w Anzahl der Kerning-Paare (0: keine Kerning-Paare vorhanden; < 0: Kerning ausgeschaltet)
t_no_track			EQU	118						;.w Anzahl der Track-Kerning-Paare

t_hor 				EQU	120						;.w horizontale Ausrichtung
t_ver 				EQU	122						;.w vertikale Ausrichtung

t_base				EQU	124						;.w Oberkante<->Basislinie
t_half				EQU	126						;.w Oberkante<->Halblinie
t_descent			EQU	128						;.w Oberkante<->Buchstabenoberkante
t_bottom 			EQU	130						;.w Oberkante<->Unterkante
t_ascent 			EQU	132						;.w Oberkante<->Buchstabenunterkante
t_top 				EQU	134						;.w Oberkante<->Oberkante

t_left_off			EQU	140						;.w linker Offset bei Kursivschrift
t_whole_off 		EQU	142						;.w Verbreiterung bei Kursivschrift
t_thicken			EQU	144						;.w Verbreiterung bei Fettschrift
t_uline				EQU	146						;.w Dicke der Unterstreichung
t_ulpos				EQU	148						;.w Abstand der Unterstreichung von der Zeichenzellenoberkante

t_width				EQU	150						;.w Textbreite
t_height 			EQU	152						;.w Texthîhe
t_cwidth 			EQU	154						;.w Zellenbreite
t_cheight			EQU	156						;.w Zellenhîhe
t_point_last		EQU	158						;.w letzte eingestellte Punkthîhe oder 0, wenn vst_height() benutzt wurde

t_scale_width		EQU	160						;.l Breite in 1/65536 Pixeln (relativ) fÅr die Zeichengenerierung
t_scale_height		EQU	164						;.l Hîhe in 1/65536 Pixeln fÅr die Zeichengenerierung
t_rotation			EQU	168						;.w Textrotation
t_skew				EQU	170						;.w Neigung gegen den Uhrzeigersinn in 1/10 Grad
t_effects			EQU	172						;.w Texteffekte
t_light_pct			EQU	174						;.w Prozentangabe fÅr das Grauraster
t_light_fill		EQU	176						;.l Zeiger auf ein Grauraster fÅr helle Schrift

;Musterdarstellung
f_color				EQU	190						;.w FÅllfarbe
f_interior			EQU	192						;.w FÅlltyp
f_style				EQU	194						;.w Musterindex
f_perimeter 		EQU	196						;.w Flag fÅr Umrahmung
f_pointer			EQU	198						;.l Zeiger aufs aktuelle FÅllmuster
f_planes 			EQU	202						;.w Planeanzahl des eingestellten Musters
f_fill0				EQU	204						;.l Zeiger auf ein weiûes FÅllmuster
f_fill1				EQU	208						;.l Zeiger auf ein vollflÑchiges FÅllmuster
f_fill2				EQU	212						;.l Zeiger auf Graumuster (das erste Muster mit Index 0 ist nicht einstellbar!)
f_fill3				EQU	216						;.l Zeiger auf Schraffuren (das erste Muster mit Index 0 ist nicht einstellbar!)
f_spointer			EQU	220						;.l Zeiger aufs selbstdefinierte FÅllmuster
f_splanes 			EQU	224						;.w Planeanzahl des selbstdefinierten Musters
f_saddr				EQU	WK_LENGTH				;Hier wird das USER-DEF gespeichert

;Markerdarstellung
m_color           EQU	230						;.w Markerfarbe
m_type            EQU	232						;.w Markertyp
m_width           EQU	234						;.w Markerbreite
m_height          EQU	236						;.w Markerhîhe
m_data            EQU	238						;.l Zeiger auf die Markerdaten

t_number 			EQU	250						;.w Zeichensatznummer
t_font_type			EQU	252						;.b Art des Fonts
t_bitmap_gdos		EQU	254						;.w Flag das angibt, ob die Bitmap-Fonts Åber GDOS eingebunden wurden
t_bitmap_fonts		EQU	256						;.l Zeiger auf weiter Bitmap-Fonts
t_res_ptr1			EQU	260						;.l reserviert
t_res_ptr2			EQU	264						;.l reserviert
t_res_xyz3			EQU	268						;.w reserviert

;Zeiger fÅr Bitmap-Schriften
t_pointer			EQU	270						;.l Zeiger auf den Zeichensatz
t_fonthdr			EQU	274						;.l Zeiger auf den aktuellen Font
t_offtab 			EQU	278						;.l Zeiger auf Zeichenoffsets
t_image				EQU	282						;.l Zeiger auf Fontdaten
t_iwidth 			EQU	286						;.w Breite der Fontdaten in Bytes
t_iheight			EQU	288						;.w Hîhe der Fontdaten in Zeilen

;temporÑre Daten fÅr Bitmap-Text
t_eff_thicken		EQU	290						;.w Verbreiterung durch Effekte
t_act_line			EQU	292						;.w Startzeile im Textbuffer
t_add_length		EQU	294						;.w zusÑtzliche LÑnge bei v_justified
t_space_kind		EQU	296						;.w -1 = zeichenweise Dehnung

;Zeiger fÅr Vektorschrift
t_FONT_ptr			EQU	270						;.l Zeiger auf die aktuelle FONT-Struktur
t_asc_map			EQU	274						;.l Zeiger auf die Tabelle ASCII zu Index
t_BAT_ptr			EQU	278						;.l Zeiger auf die Attribute-Tabelle (muû nicht gÅltig sein)
t_bin_table			EQU	282						;.l Zeiger auf eine gÅltige Schnellzugriffstabelle oder 0

;Maûe fÅr Vektorschriften
t_width32			EQU	300						;.l eingestellte Breite in 1/65536 Pixeln (relativer Wert)
t_height32			EQU	304						;.l eingestellte Hîhe in 1/65536 Pixeln
t_point_width		EQU	308						;.l Breite in 1/65536 Punkten
t_point_height 	EQU	312						;.l Hîhe in 1/65536 Punkten

t_track_index		EQU	316						;.w Nummer des Track-Index
t_track_offset		EQU	318						;.l Offset zwischen mehreren Zeichen in 1/65536 Pixel	

t_left_off32		EQU	322						;.l linker Offset bei Kursivschrift
t_whole_off32		EQU	326						;.l Verbreiterung bei Kursivschrift
t_thicken32			EQU	330						;.l Verbreiterung bei Fettschrift

;temporÑre Daten fÅr Vektorschrift
t_thicken_x			EQU	340						;.l x-Anteil der Verbreiterung
t_thicken_y			EQU	344						;.l y-Anteil der Verbreiterung
t_char_x				EQU	348						;.l x-Anteil
t_char_y				EQU	352						;.l y-Anteil
t_word_x				EQU	356						;.l x-Anteil
t_word_y				EQU	360						;.l y-Anteil
t_string_x			EQU	364						;.l x-Anteil der String-Breite
t_string_y			EQU	368						;.l y-Anteil der String-Breite
t_last_x				EQU	372						;.l x-Anteil der Breite des letzten Zeichens
t_last_y				EQU	376						;.l y-Anteil der Breite des letzten Zeichens

t_gtext_spacing	EQU	380						;.w Gibt an, ob Zeichenweiten wie bei v_gtext benutzt werden sollen

t_xadd				EQU	382						;.w
t_yadd				EQU	384						;.w

;die beiden nÑchsten Variablen werden in der schnellen Textroutine benutzt
t_buf_x1				EQU	386						;.w x1 der Bitmap innerhalb des Textbuffers
t_buf_x2				EQU	388						;.w x2 der Bitmap innerhalb des Textbuffers

;Bitmap-Beschreibung
device_drvr			EQU	400						;.l Zeiger auf auf die GerÑtetreiber-Struktur (DEVICE_DRIVER) oder 0L
bitmap_drvr			EQU	404						;.l Zeiger auf den Offscreen-Treiber-Struktur (DRIVER)
bitmap_info			EQU	410						;ORGANISATION-Struktur
bitmap_colors		EQU	410						;.l gleichzeitig darstellbare Farben
bitmap_planes		EQU	414						;.w Anzahl der Planes
bitmap_format		EQU	416						;.w Pixelformat
bitmap_flags		EQU	418						;.w Bitreihenfolge (wenn das oberste Bit gesetzt ist, wurde fÅr die Bitmap Speicher angefordert)
bitmap_res1			EQU	420						;.w reserviert
bitmap_res2			EQU	422						;.w reserviert
bitmap_res3			EQU	424						;.w reserviert

bitmap_addr 		EQU	430						;.l Zeiger auf die Bitmap oder 0L bei Bildschirmtreibern
bitmap_width		EQU	434						;.w Bytes pro Zeile oder 0 bei Bildschirmtreibern
planes				EQU	436						;.w Anzahl der Bildebenen -1
bitmap_off_x		EQU	438						;.w x-Offset fÅr Koordinaten
bitmap_off_y		EQU	440						;.w y-Offset fÅr Koordinaten
bitmap_dx			EQU	442						;.w Breite der Bitmap - 1
bitmap_dy			EQU	444						;.w Hîhe der Bitmap - 1
bitmap_len			EQU	446						;.l LÑnge der Bitmap in Bytes

;Rasteroperationen
r_saddr				EQU	450						;.l Quelladresse
r_swidth 			EQU	454						;.w Bytes pro Quellzeile
r_splanes			EQU	456						;.w Anzahl der Quellebenen - 1
r_splane_len		EQU	458						;.l LÑnge der kompletten Bitmap (d.h. LÑnge aller Ebenen!)
r_snxtword			EQU	458						;.w alternativ Abstand zum nÑchsten Wort der gleichen Ebene

r_daddr				EQU	470						;.l Zieladresse
r_dwidth 			EQU	474						;.w Bytes pro Zielzeile
r_dplanes			EQU	476						;.w Anzahl der Zielebenen - 1
r_dplane_len		EQU	478						;.l LÑnge der kompletten Bitmap (d.h. LÑnge aller Ebenen!)
r_dnxtword			EQU	478						;.w alternativ Abstand zum nÑchsten Wort der gleichen Ebene

r_fgcol				EQU	490						;.w Vordergrundfarbe
r_bgcol				EQU	492						;.w Hintergrundfarbe
r_wmode				EQU	494						;.w VerknÅpfung

;Zeiger auf die Sprungtabellen
p_fbox				EQU	500						;.l Vektor fÅr gefÅlltes Rechteck
p_fline				EQU	504						;.l Vektor fÅr gefÅllte Linie
p_hline				EQU	508						;.l Vektor fÅr horizontale Linie
p_vline				EQU	512						;.l Vektor fÅr vertikale Linie
p_line				EQU	516						;.l Vektor fÅr schrÑge Linie
p_expblt	 			EQU	520						;.l Vektor fÅr expandierenden Bitblocktransfer
p_bitblt	 			EQU	524						;.l Vektor fÅr Bitblocktransfer
p_textblt			EQU	528						;.l Vektor fÅr Textblt
p_scanline			EQU	532						;.l Vektor fÅr Scanline (Seedfill)

p_set_pixel			EQU	536						;.l 
p_get_pixel			EQU	540						;.l 
p_transform			EQU	544						;.l 
p_set_pattern		EQU	548						;.l 
p_set_color_rgb	EQU	552						;.l 
p_get_color_rgb	EQU	556						;.l 
p_vdi_to_color		EQU	560						;.l 
p_color_to_vdi		EQU	564						;.l 

p_gtext				EQU	580						;.l 
p_escapes			EQU	584						;.l 

wk_owner				EQU	596						;.l Zeiger auf die Applikation, die zur WK gehîrt

;color_map_ptr
;color_remap_ptr


