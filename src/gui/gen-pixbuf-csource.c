/* Gdk-Pixbuf-CSource - GdkPixbuf based image CSource generator
 * Copyright (C) 1999, 2001 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#define GDK_DISABLE_DEPRECATION_WARNINGS
#define GTK_DISABLE_DEPRECATION_WARNINGS
#define GLIB_DISABLE_DEPRECATION_WARNINGS
#define GDK_PIXBUF_DISABLE_DEPRECATION_WARNINGS

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <glib/gprintf.h>
#pragma GCC diagnostic pop
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif

#define GDK_PIXDATA_DUMP_BYTES (GDK_PIXDATA_DUMP_CONST << 1)


/* --- defines --- */
#undef	G_LOG_DOMAIN
#define	G_LOG_DOMAIN	"Gdk-Pixbuf-CSource"
#define PRG_NAME        "gen-pixbuf-csource"
#define PKG_NAME        "gdk-pixbuf"
#define PKG_HTTP_HOME   "http://www.gtk.org"


/* --- variables --- */
static guint gen_type = GDK_PIXDATA_DUMP_PIXDATA_STREAM;
static guint gen_ctype = GDK_PIXDATA_DUMP_GTYPES | GDK_PIXDATA_DUMP_STATIC | GDK_PIXDATA_DUMP_CONST | GDK_PIXDATA_DUMP_BYTES;
static gboolean use_rle = TRUE;
static gboolean with_decoder = FALSE;
static gchar *image_name = "my_pixbuf";
static gboolean build_list = FALSE;


/* --- functions --- */


#define APPEND g_string_append_printf

/* --- functions --- */
static guint pixdata_get_length(const GdkPixdata * pixdata)
{
	guint bpp, length;

	if ((pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGB)
		bpp = 3;
	else if ((pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGBA)
		bpp = 4;
	else
		return 0;						/* invalid format */
	switch (pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK)
	{
		guint8 *rle_buffer;
		guint max_length;

	case GDK_PIXDATA_ENCODING_RAW:
		length = pixdata->rowstride * pixdata->height;
		break;
	case GDK_PIXDATA_ENCODING_RLE:
		/* need an RLE walk to determine size */
		max_length = pixdata->rowstride * pixdata->height;
		rle_buffer = pixdata->pixel_data;
		length = 0;
		while (length < max_length)
		{
			guint chunk_length = *(rle_buffer++);

			if (chunk_length & 128)
			{
				chunk_length = chunk_length - 128;
				if (!chunk_length)		/* RLE data corrupted */
					return 0;
				length += chunk_length * bpp;
				rle_buffer += bpp;
			} else
			{
				if (!chunk_length)		/* RLE data corrupted */
					return 0;
				chunk_length *= bpp;
				length += chunk_length;
				rle_buffer += chunk_length;
			}
		}
		length = rle_buffer - pixdata->pixel_data;
		break;
	default:
		length = 0;
		break;
	}
	return length;
}


typedef struct
{
	/* config */
	gboolean dump_stream;
	gboolean dump_struct;
	gboolean dump_macros;
	gboolean dump_gtypes;
	gboolean dump_bytes;
	gboolean dump_rle_decoder;
	const gchar *static_prefix;
	const gchar *const_prefix;
	/* runtime */
	GString *gstring;
	guint pos;
	gboolean pad;
} CSourceData;

#define NEWLINE(cdata) \
	if (!(cdata)->dump_bytes && (cdata)->pos != 0) APPEND(gstring, "\""); \
	if ((cdata)->dump_macros) APPEND(gstring, " \\"); \
	APPEND(gstring, "\n"); \
	if ((cdata)->pos != 0) APPEND(gstring, "  "); \
	(cdata)->pos = 2
	
static inline void save_uchar(CSourceData *cdata, guint8 d, int last)
{
	GString *gstring = cdata->gstring;

	if (cdata->pos > 94)
	{
		if (cdata->dump_struct || cdata->dump_stream || cdata->dump_macros)
		{
			NEWLINE(cdata);
			if (!(cdata)->dump_bytes) APPEND(gstring, "\"");
			cdata->pad = FALSE;
		}
	}
	if (cdata->dump_bytes)
	{
		APPEND(gstring, "0x%02x", d);
		cdata->pos += 4;
		if (cdata->dump_bytes && !last)
		{
			APPEND(cdata->gstring, ", ");
			cdata->pos += 2;
		}
		cdata->pad = FALSE;
		return;
	}
	if (d < 33 || d > 126 || d == '?')
	{
		APPEND(gstring, "\\%o", d);
		cdata->pos += 1 + 1 + (d > 7) + (d > 63);
		cdata->pad = d < 64;
		return;
	}
	if (d == '\\')
	{
		g_string_append(gstring, "\\\\");
		cdata->pos += 2;
	} else if (d == '"')
	{
		g_string_append(gstring, "\\\"");
		cdata->pos += 2;
	} else if (cdata->pad && d >= '0' && d <= '9')
	{
		g_string_append(gstring, "\"\"");
		g_string_append_c(gstring, d);
		cdata->pos += 3;
	} else
	{
		g_string_append_c(gstring, d);
		cdata->pos += 1;
	}
	cdata->pad = FALSE;
}


static inline void
 save_rle_decoder(GString * gstring,
				  const gchar * macro_name,
				  const gchar * s_uint,
				  const gchar * s_uint_8)
{
	APPEND(gstring, "#define %s_RUN_LENGTH_DECODE(image_buf, rle_data, size, bpp) do \\\n",
		   macro_name);
	APPEND(gstring, "{ %s __bpp; %s *__ip; const %s *__il, *__rd; \\\n", s_uint, s_uint_8, s_uint_8);
	APPEND(gstring, "  __bpp = (bpp); __ip = (image_buf); __il = __ip + (size) * __bpp; \\\n");

	APPEND(gstring, "  __rd = (rle_data); if (__bpp > 3) { /* RGBA */ \\\n");

	APPEND(gstring, "    while (__ip < __il) { %s __l = *(__rd++); \\\n", s_uint);
	APPEND(gstring, "      if (__l & 128) { __l = __l - 128; \\\n");
	APPEND(gstring, "        do { memcpy (__ip, __rd, 4); __ip += 4; } while (--__l); __rd += 4; \\\n");
	APPEND(gstring, "      } else { __l *= 4; memcpy (__ip, __rd, __l); \\\n");
	APPEND(gstring, "               __ip += __l; __rd += __l; } } \\\n");

	APPEND(gstring, "  } else { /* RGB */ \\\n");

	APPEND(gstring, "    while (__ip < __il) { %s __l = *(__rd++); \\\n", s_uint);
	APPEND(gstring, "      if (__l & 128) { __l = __l - 128; \\\n");
	APPEND(gstring, "        do { memcpy (__ip, __rd, 3); __ip += 3; } while (--__l); __rd += 3; \\\n");
	APPEND(gstring, "      } else { __l *= 3; memcpy (__ip, __rd, __l); \\\n");
	APPEND(gstring, "               __ip += __l; __rd += __l; } } \\\n");

	APPEND(gstring, "  } } while (0)\n");
}


static void gen_pixbuf_data(CSourceData *cdata, guint8 *img_buffer, guint8 *img_buffer_end)
{
	do
	{
		guint8 d = *img_buffer++;
		save_uchar(cdata, d, img_buffer >= img_buffer_end);
	} while (img_buffer < img_buffer_end);
}


/**
 * gdk_pixdata_to_csource:
 * @pixdata: a #GdkPixdata to convert to C source.
 * @name: used for naming generated data structures or macros.
 * @dump_type: a #GdkPixdataDumpType determining the kind of C
 *   source to be generated.
 *
 * Generates C source code suitable for compiling images directly 
 * into programs. 
 *
 * gdk-pixbuf ships with a program called <command>gdk-pixbuf-csource</command> 
 * which offers a command line interface to this function.
 *
 * Returns: a newly-allocated string containing the C source form
 *   of @pixdata.
 **/
static GString *gen_pixdata_to_csource(GdkPixdata *pixdata, const gchar *name, GdkPixdataDumpType dump_type)
{
	CSourceData cdata =	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	const char *s_uint_8;
	guint bpp, width, height, rowstride;
	gboolean rle_encoded;
	gchar *macro_name;
	guint8 *img_buffer, *img_buffer_end, *stream = NULL;
	guint stream_length;
	GString *gstring;

	/* check args passing */
	g_return_val_if_fail(pixdata != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);
	/* check pixdata contents */
	g_return_val_if_fail(pixdata->magic == GDK_PIXBUF_MAGIC_NUMBER, NULL);
	g_return_val_if_fail(pixdata->width > 0, NULL);
	g_return_val_if_fail(pixdata->height > 0, NULL);
	g_return_val_if_fail(pixdata->rowstride >= pixdata->width, NULL);
	g_return_val_if_fail((pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGB ||
						 (pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGBA, NULL);
	g_return_val_if_fail((pixdata->pixdata_type & GDK_PIXDATA_SAMPLE_WIDTH_MASK) == GDK_PIXDATA_SAMPLE_WIDTH_8, NULL);
	g_return_val_if_fail((pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK) == GDK_PIXDATA_ENCODING_RAW ||
						 (pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK) == GDK_PIXDATA_ENCODING_RLE, NULL);
	g_return_val_if_fail(pixdata->pixel_data != NULL, NULL);

	img_buffer = pixdata->pixel_data;
	if (pixdata->length < 1)
		img_buffer_end = img_buffer + pixdata_get_length(pixdata);
	else
		img_buffer_end = img_buffer + pixdata->length - GDK_PIXDATA_HEADER_LENGTH;
	g_return_val_if_fail(img_buffer < img_buffer_end, NULL);

	bpp = (pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGB ? 3 : 4;
	width = pixdata->width;
	height = pixdata->height;
	rowstride = pixdata->rowstride;
	rle_encoded = (pixdata->pixdata_type & GDK_PIXDATA_ENCODING_RLE) > 0;
	macro_name = g_ascii_strup(name, -1);

	cdata.dump_macros = (dump_type & GDK_PIXDATA_DUMP_MACROS) > 0;
	cdata.dump_struct = (dump_type & GDK_PIXDATA_DUMP_PIXDATA_STRUCT) > 0;
	cdata.dump_stream = !cdata.dump_macros && !cdata.dump_struct;
	g_return_val_if_fail(cdata.dump_macros + cdata.dump_struct + cdata.dump_stream == 1, NULL);

	cdata.dump_gtypes = (dump_type & GDK_PIXDATA_DUMP_CTYPES) == 0;
	cdata.dump_bytes = (dump_type & GDK_PIXDATA_DUMP_BYTES) != 0;
	if (cdata.dump_macros)
		cdata.dump_bytes = 0;
	cdata.dump_rle_decoder = (dump_type & GDK_PIXDATA_DUMP_RLE_DECODER) > 0;
	cdata.static_prefix = (dump_type & GDK_PIXDATA_DUMP_STATIC) ? "static " : "";
	cdata.const_prefix = (dump_type & GDK_PIXDATA_DUMP_CONST) ? "const " : "";
	gstring = g_string_new(NULL);
	cdata.gstring = gstring;

	s_uint_8 = cdata.dump_gtypes ? "guint8" : "unsigned char";

	/* initial comment
	   */
	APPEND(gstring,
		   "/* GdkPixbuf %s C-Source image dump %s*/\n\n",
		   bpp > 3 ? "RGBA" : "RGB",
		   rle_encoded ? "1-byte-run-length-encoded " : "");

	/* dump RLE decoder for structures
	   */
	if (cdata.dump_rle_decoder && cdata.dump_struct)
		save_rle_decoder(gstring,
						 macro_name,
						 cdata.dump_gtypes ? "guint" : "unsigned int",
						 cdata.dump_gtypes ? "guint8" : "unsigned char");

	/* format & size blurbs
	   */
	if (cdata.dump_macros)
	{
		APPEND(gstring, "#define %s_ROWSTRIDE (%u)\n",
			   macro_name, rowstride);
		APPEND(gstring, "#define %s_WIDTH (%u)\n",
			   macro_name, width);
		APPEND(gstring, "#define %s_HEIGHT (%u)\n",
			   macro_name, height);
		APPEND(gstring, "#define %s_BYTES_PER_PIXEL (%u) /* 3:RGB, 4:RGBA */\n",
			   macro_name, bpp);
	}
	if (cdata.dump_struct)
	{
		if (cdata.dump_bytes)
		{
 			APPEND(gstring, "%s%s%s %s_data = {\n  ", cdata.static_prefix, cdata.const_prefix, s_uint_8, name);
			gen_pixbuf_data(&cdata, img_buffer, img_buffer_end);
			APPEND(gstring, "\n};\n\n");
		}
		
		APPEND(gstring, "%s%sGdkPixdata %s = {\n",
			   cdata.static_prefix, cdata.const_prefix, name);
		APPEND(gstring, "  0x%x, /* Pixbuf magic: 'GdkP' */\n",
			   GDK_PIXBUF_MAGIC_NUMBER);
		APPEND(gstring, "  %d + %lu, /* header length + pixel_data length */\n",
			   GDK_PIXDATA_HEADER_LENGTH,
			   rle_encoded ? (glong) (img_buffer_end - img_buffer) : (glong) (rowstride * height));
		APPEND(gstring, "  0x%x, /* pixdata_type */\n",
			   pixdata->pixdata_type);
		APPEND(gstring, "  %u, /* rowstride */\n",
			   rowstride);
		APPEND(gstring, "  %u, /* width */\n",
			   width);
		APPEND(gstring, "  %u, /* height */\n",
			   height);
		APPEND(gstring, "  /* pixel_data: */\n");
	}
	if (cdata.dump_stream)
	{
		guint pix_length = img_buffer_end - img_buffer;

		stream = gdk_pixdata_serialize(pixdata, &stream_length);
		img_buffer = stream;
		img_buffer_end = stream + stream_length;

		APPEND(gstring, "#ifdef __SUNPRO_C\n");
		APPEND(gstring, "#pragma align 4 (%s)\n", name);
		APPEND(gstring, "#endif\n");

		APPEND(gstring, "#ifdef __GNUC__\n");
		APPEND(gstring, "__extension__ %s%s%s %s[] __attribute__ ((__aligned__ (4))) = \n",
			   cdata.static_prefix, cdata.const_prefix,
			   cdata.dump_gtypes ? "guint8" : "unsigned char",
			   name);
		APPEND(gstring, "#else\n");
		APPEND(gstring, "%s%s%s %s[] = \n",
			   cdata.static_prefix, cdata.const_prefix,
			   cdata.dump_gtypes ? "guint8" : "unsigned char",
			   name);
		APPEND(gstring, "#endif\n");

		APPEND(gstring, "{ \n  /* Pixbuf magic (0x%x) */",
			   GDK_PIXBUF_MAGIC_NUMBER);
		cdata.pos = 3;
		NEWLINE(&cdata);
		if (!cdata.dump_bytes) APPEND(gstring, "\"");
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		NEWLINE(&cdata);
		APPEND(gstring, "/* length: header (%d) + pixel_data (%u) */",
			   GDK_PIXDATA_HEADER_LENGTH,
			   rle_encoded ? pix_length : rowstride * height);
		NEWLINE(&cdata);
		if (!cdata.dump_bytes) APPEND(gstring, "\"");
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		NEWLINE(&cdata);
		APPEND(gstring, "/* pixdata_type (0x%x) */",
			   pixdata->pixdata_type);
		NEWLINE(&cdata);
		if (!cdata.dump_bytes) APPEND(gstring, "\"");
		cdata.pos = 3;
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		NEWLINE(&cdata);
		APPEND(gstring, "/* rowstride (%u) */",
			   rowstride);
		NEWLINE(&cdata);
		if (!cdata.dump_bytes) APPEND(gstring, "\"");
		cdata.pos = 3;
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		NEWLINE(&cdata);
		APPEND(gstring, "/* width (%u) */", width);
		NEWLINE(&cdata);
		if (!cdata.dump_bytes) APPEND(gstring, "\"");
		cdata.pos = 3;
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		NEWLINE(&cdata);
		APPEND(gstring, "/* height (%u) */", height);
		NEWLINE(&cdata);
		if (!cdata.dump_bytes) APPEND(gstring, "\"");
		cdata.pos = 3;
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		save_uchar(&cdata, *img_buffer++, FALSE);
		NEWLINE(&cdata);
		APPEND(gstring, "/* pixel_data: */\n");
		cdata.pos = 3;
	}
	/* pixel_data intro
	   */
	if (cdata.dump_macros)
	{
		APPEND(gstring, "#define %s_%sPIXEL_DATA ((%s*) \\\n",
			   macro_name,
			   rle_encoded ? "RLE_" : "",
			   s_uint_8);
		APPEND(gstring, "  ");
		if (!cdata.dump_bytes) 
			APPEND(gstring, "\"");
		cdata.pos = 2;
	}
	else if (cdata.dump_struct)
	{
		APPEND(gstring, "  ");
		if (cdata.dump_bytes) 
 			APPEND(gstring, "%s_data", name);
 		else
			APPEND(gstring, "\"");
		cdata.pos = 3;
	}
	else if (cdata.dump_stream)
	{
		APPEND(gstring, "  ");
		if (!cdata.dump_bytes) 
			APPEND(gstring, "\"");
		cdata.pos = 3;
	} else
	{
		APPEND(gstring, "  ");
		cdata.pos = 2;
	}
	/* pixel_data
	   */
	if (!(cdata.dump_struct && cdata.dump_bytes))
	{
		gen_pixbuf_data(&cdata, img_buffer, img_buffer_end);
		if (!cdata.dump_bytes) 
			APPEND(gstring, "\"");
	}
	
	/* pixel_data trailer
	   */
	if (cdata.dump_macros)
		APPEND(gstring, ")\n\n");
	if (cdata.dump_struct)
		APPEND(gstring, "\n};\n\n");
	if (cdata.dump_stream)
		APPEND(gstring, "\n};\n\n");

	/* dump RLE decoder for macros
	   */
	if (cdata.dump_rle_decoder && cdata.dump_macros)
		save_rle_decoder(gstring,
						 macro_name,
						 cdata.dump_gtypes ? "guint" : "unsigned int",
						 cdata.dump_gtypes ? "guint8" : "unsigned char");

	/* cleanup
	   */
	g_free(stream);
	g_free(macro_name);

	return gstring;
}



static void print_csource(FILE *f_out, GdkPixbuf *pixbuf)
{
	GdkPixdata pixdata;
	gpointer free_me;
	GString *gstring;

	free_me = gdk_pixdata_from_pixbuf(&pixdata, pixbuf, use_rle);
	gstring = gen_pixdata_to_csource(&pixdata, image_name, (GdkPixdataDumpType)(gen_type | gen_ctype | (with_decoder ? GDK_PIXDATA_DUMP_RLE_DECODER : 0)));

	g_fprintf(f_out, "%s\n", gstring->str);

	g_free(free_me);
}



static void print_version(FILE *bout)
{
	g_fprintf(bout, "%s version ", PRG_NAME);
	g_fprintf(bout, "%s", GDK_PIXBUF_VERSION);
	g_fprintf(bout, "\n");
	g_fprintf(bout, "%s comes with ABSOLUTELY NO WARRANTY.\n", PRG_NAME);
	g_fprintf(bout, "You may redistribute copies of %s under the terms of\n", PRG_NAME);
	g_fprintf(bout, "the GNU Lesser General Public License which can be found in the\n");
	g_fprintf(bout, "%s source package. Sources, examples and contact\n", PKG_NAME);
	g_fprintf(bout, "information are available at %s\n", PKG_HTTP_HOME);
}


static void print_usage(FILE *bout)
{
	g_fprintf(bout, "Usage: %s [options] [image]\n", PRG_NAME);
	g_fprintf(bout, "       %s [options] --build-list [[name image]...]\n", PRG_NAME);
	g_fprintf(bout, "  --stream                   generate pixbuf data stream\n");
	g_fprintf(bout, "  --struct                   generate GdkPixdata structure\n");
	g_fprintf(bout, "  --macros                   generate image size/pixel macros\n");
	g_fprintf(bout, "  --rle                      use one byte run-length-encoding\n");
	g_fprintf(bout, "  --raw                      provide raw image data copy\n");
	g_fprintf(bout, "  --extern                   generate extern symbols\n");
	g_fprintf(bout, "  --static                   generate static symbols\n");
	g_fprintf(bout, "  --bytes                    generate byte array for data\n");
	g_fprintf(bout, "  --string                   generate string constant for data\n");
	g_fprintf(bout, "  --decoder                  provide rle decoder\n");
	g_fprintf(bout, "  --name=identifier          C macro/variable name\n");
	g_fprintf(bout, "  --build-list               parse (name, image) pairs\n");
	g_fprintf(bout, "  -h, --help                 show this help message\n");
	g_fprintf(bout, "  -v, --version              print version informations\n");
	g_fprintf(bout, "  --g-fatal-warnings         make warnings fatal (abort)\n");
}


static void parse_args(gint *argc_p, gchar ***argv_p)
{
	guint argc = *argc_p;
	gchar **argv = *argv_p;
	guint i, e;

	for (i = 1; i < argc; i++)
	{
		if (strcmp("--macros", argv[i]) == 0)
		{
			gen_type = GDK_PIXDATA_DUMP_MACROS;
			argv[i] = NULL;
		} else if (strcmp("--struct", argv[i]) == 0)
		{
			gen_type = GDK_PIXDATA_DUMP_PIXDATA_STRUCT;
			argv[i] = NULL;
		} else if (strcmp("--stream", argv[i]) == 0)
		{
			gen_type = GDK_PIXDATA_DUMP_PIXDATA_STREAM;
			argv[i] = NULL;
		} else if (strcmp("--rle", argv[i]) == 0)
		{
			use_rle = TRUE;
			argv[i] = NULL;
		} else if (strcmp("--raw", argv[i]) == 0)
		{
			use_rle = FALSE;
			argv[i] = NULL;
		} else if (strcmp("--extern", argv[i]) == 0)
		{
			gen_ctype &= ~GDK_PIXDATA_DUMP_STATIC;
			argv[i] = NULL;
		} else if (strcmp("--static", argv[i]) == 0)
		{
			gen_ctype |= GDK_PIXDATA_DUMP_STATIC;
			argv[i] = NULL;
		} else if (strcmp("--decoder", argv[i]) == 0)
		{
			with_decoder = TRUE;
			argv[i] = NULL;
		} else if (strcmp("--bytes", argv[i]) == 0)
		{
			gen_ctype |= GDK_PIXDATA_DUMP_BYTES;
			argv[i] = NULL;
		} else if (strcmp("--string", argv[i]) == 0)
		{
			gen_ctype &= ~GDK_PIXDATA_DUMP_BYTES;
			argv[i] = NULL;
		} else if ((strcmp("--name", argv[i]) == 0) ||
				   (strncmp("--name=", argv[i], 7) == 0))
		{
			gchar *equal = argv[i] + 6;

			if (*equal == '=')
			{
				image_name = g_strdup(equal + 1);
			} else if (i + 1 < argc)
			{
				image_name = g_strdup(argv[i + 1]);
				argv[i] = NULL;
				i += 1;
			}
			argv[i] = NULL;
		} else if (strcmp("--build-list", argv[i]) == 0)
		{
			build_list = TRUE;
			argv[i] = NULL;
		} else if (strcmp("-h", argv[i]) == 0 ||
				   strcmp("--help", argv[i]) == 0)
		{
			print_usage(stdout);
			argv[i] = NULL;
			exit(EXIT_SUCCESS);
		} else if (strcmp("-v", argv[i]) == 0 ||
				   strcmp("--version", argv[i]) == 0)
		{
			print_version(stdout);
			argv[i] = NULL;
			exit(EXIT_SUCCESS);
		} else if (strcmp(argv[i], "--g-fatal-warnings") == 0)
		{
			GLogLevelFlags fatal_mask;

			fatal_mask = g_log_set_always_fatal((GLogLevelFlags)G_LOG_FATAL_MASK);
			fatal_mask = (GLogLevelFlags)(fatal_mask | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);
			g_log_set_always_fatal(fatal_mask);

			argv[i] = NULL;
		}
	}

	e = 0;
	for (i = 1; i < argc; i++)
	{
		if (e)
		{
			if (argv[i])
			{
				argv[e++] = argv[i];
				argv[i] = NULL;
			}
		} else if (!argv[i])
		{
			e = i;
		}
	}
	if (e)
		*argc_p = e;
}


int main(int argc, char *argv[])
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	gchar *infilename;

	/* initialize glib/GdkPixbuf */
	g_type_init();

	/* parse args and do fast exits */
	parse_args(&argc, &argv);

#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#endif
#endif

#ifdef O_BINARY
	setmode(0, O_BINARY);
	setmode(1, O_BINARY);
#endif

	if (!build_list)
	{
		if (argc != 2)
		{
			print_usage(stderr);
			return EXIT_FAILURE;
		}
#ifdef G_OS_WIN32
		infilename = g_locale_to_utf8(argv[1], -1, NULL, NULL, NULL);
#else
		infilename = argv[1];
#endif	/* G_OS_WIN32 */

		pixbuf = gdk_pixbuf_new_from_file(infilename, &error);
		if (!pixbuf)
		{
			g_fprintf(stderr, "failed to load \"%s\": %s\n",
					  argv[1],
					  error->message);
			g_error_free(error);
			return EXIT_FAILURE;
		}
		print_csource(stdout, pixbuf);
		g_object_unref(pixbuf);
	} else
		/* parse name, file pairs */
	{
		gchar **p = argv + 1;
		guint j = argc - 1;
		gboolean toggle = FALSE;

		while (j--)
		{
#ifdef G_OS_WIN32
			infilename = g_locale_to_utf8(*p, -1, NULL, NULL, NULL);
#else	/* G_OS_WIN32 */
			infilename = *p;
#endif	/* G_OS_WIN32 */

			if (!toggle)
			{
				image_name = infilename;
				p++;
			} else
			{
				pixbuf = gdk_pixbuf_new_from_file(infilename, &error);
				if (!pixbuf)
				{
					g_fprintf(stderr, "failed to load \"%s\": %s\n",
							  *p,
							  error->message);
					g_error_free(error);
					return EXIT_FAILURE;
				}
				print_csource(stdout, pixbuf);
				g_object_unref(pixbuf);
				p++;
			}
			toggle = !toggle;
		}
	}

	return EXIT_SUCCESS;
}
