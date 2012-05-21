#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include "dxt.h"
#include "vtf.h"

/* Flags */
#define VTF_FLAG_POINTSAMPLE			0x00000001
#define VTF_FLAG_TRILINEAR				0x00000002
#define VTF_FLAG_CLAMPS					0x00000004
#define VTF_FLAG_CLAMPT					0x00000008
#define VTF_FLAG_ANISOTROPIC			0x00000010
#define VTF_FLAG_HINT_DXT5				0x00000020
#define VTF_FLAG_PWL_CORRECTED			0x00000040
#define VTF_FLAG_NORMAL					0x00000080
#define VTF_FLAG_NOMIP					0x00000100
#define VTF_FLAG_NOLOD					0x00000200
#define VTF_FLAG_ALL_MIPS				0x00000400
#define VTF_FLAG_PROCEDURAL				0x00000800
#define VTF_FLAG_ONEBITALPHA			0x00001000
#define VTF_FLAG_EIGHTBITALPHA			0x00002000
#define VTF_FLAG_ENVMAP					0x00004000
#define VTF_FLAG_RENDERTARGET			0x00008000
#define VTF_FLAG_DEPTHRENDERTARGET		0x00010000
#define VTF_FLAG_NODEBUGOVERRIDE		0x00020000
#define VTF_FLAG_SINGLECOPY				0x00040000
#define VTF_FLAG_PRE_SRGB				0x00080000
#define VTF_FLAG_NODEPTHBUFFER			0x00800000
#define VTF_FLAG_CLAMPU					0x02000000
#define VTF_FLAG_VERTEXTEXTURE			0x04000000
#define VTF_FLAG_SSBUMP					0x08000000
#define VTF_FLAG_BORDER					0x20000000

/* Resource type */
#define VTF_RESOURCE_LORES_IMAGE		0x00000001
#define VTF_RESOURCE_HIRES_IMAGE		0x00000030


#define VTF_VERSION(vtf, major, minor)	\
	(hdr.version[0] >= major && hdr.version[1] >= minor)



typedef struct _VtfHeader {
	gchar		magic[4];				/* file signature ("VTF\0") */
	guint		version[2];				/* version[0].version[1] (currently 7.2) */
	guint		header_size;			/* size of the header struct (16 byte aligned; currently 80 bytes) */
	
	guint16		width;					/* Width of the largest mipmap in pixels. Must be a power of 2 */
	guint16		height;					/* Height of the largest mipmap in pixels. Must be a power of 2 */
	guint		flags;					/* VTF flags */
	guint16		frames;					/* Number of frames, if animated (1 for no animation) */
	guint16		first_frame;			/* First frame in animation (0 based) */
	guint8		padding0[4];
	
	gfloat		reflectivity[3];		/* reflectivity vector */
	guint8		padding1[4];
	
	/* version 7.0 */
	gfloat		bumpmap_scale;			/* Bumpmap scale */
	guint		highres_image_format;	/* High resolution image format */
	guint8		mipmap_count;			/* Number of mipmaps */
	guint		lowres_image_format;	/* Low resolution image format (always DXT1) */
	guint8		lowres_image_width;		/* Low resolution image width */
	guint8		lowres_image_height;	/* Low resolution image height */
	
	/* version 7.2 */
	guint16		depth;					/* Depth of the largest mipmap in pixels */
	
	/* version 7.3 */
	guint8		padding2[3];
	guint		resource_count;
	guint32		padding[2];
} __attribute__((packed)) VtfHeader;

typedef struct _VtfResource {
	guint32		type;
	guint32 	data;
} VtfResource;

struct _Vtf {
	guint width;
	guint height;
	guint format;
	GPtrArray *images;
};


GQuark
vtf_error_quark (void)
{
	return g_quark_from_static_string ("vtf-error-quark");
}


static gboolean
is_power_of_two (guint16 n)
{
	static const guint numbers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
			1024, 2048, 4096, 8192, 16384, 32768};
	gint i;
	for (i = 0; i <= 16; i++)
		if (numbers[i] == n)
			return TRUE;
	return FALSE;
}


static guint
calc_mipmap_size (guint size, guint mipmap)
{
	if (size > 1) {
		while (mipmap > 0) {
			size /= 2;
			mipmap--;
		}
	}
	
	return size;
}


static guint
get_image_length (guint format, guint width, guint height)
{
	int npixels = width * height;
	switch (format) {
	case VTF_FORMAT_RGBA8888:
	case VTF_FORMAT_ABGR8888:
	case VTF_FORMAT_ARGB8888:
	case VTF_FORMAT_BGRA8888:
	case VTF_FORMAT_BGRX8888:
		return npixels * 4;
	case VTF_FORMAT_RGB888:
	case VTF_FORMAT_BGR888:
	case VTF_FORMAT_RGB888_BLUESCREEN:
	case VTF_FORMAT_BGR888_BLUESCREEN:
		return npixels * 3;
	case VTF_FORMAT_RGB565:
	case VTF_FORMAT_BGR565:
	case VTF_FORMAT_BGRX5551:
	case VTF_FORMAT_BGRA4444:
	case VTF_FORMAT_BGRA5551:
		return npixels * 2;
	case VTF_FORMAT_DXT1:
		return npixels < 16 ? 8 : npixels / 2;
	case VTF_FORMAT_DXT3:
	case VTF_FORMAT_DXT5:
		return npixels < 16 ? 16 : npixels;
	default:
		return 0;
	}
}


Vtf *
vtf_open_mem (gpointer data, gsize length, GError **error)
{
	gpointer p = data;
	
	Vtf *vtf = g_new0 (Vtf, 1);
	vtf->images = g_ptr_array_new_with_free_func (g_free);
	
	VtfHeader hdr;
	memcpy (&hdr, p, sizeof (hdr));
	p += sizeof (hdr);
	
	if (!(is_power_of_two (hdr.width) && is_power_of_two (hdr.height) &&
			is_power_of_two (hdr.lowres_image_width) &&
			is_power_of_two (hdr.lowres_image_height))) {
		g_set_error (error, VTF_ERROR, VTF_ERROR_DIMENTION,
				"Dimensions of the image are not power of 2");
		goto err;
	}
	
	if (!VTF_VERSION (vtf, 7, 2)) {
		if (hdr.depth == 0) {
			g_set_error (error, VTF_ERROR, VTF_ERROR_INVALID,
					"Texture depth equals 0");
			goto err;
		}
	} else {
		hdr.depth = 1;
	}
	
	if (hdr.depth > 1) {
		g_set_error (error, VTF_ERROR, VTF_ERROR_UNSUPPORTED,
				"3D textures are not supported");
		goto err;
	}
	
	if (hdr.mipmap_count == 0) {
		g_set_error (error, VTF_ERROR, VTF_ERROR_INVALID,
				"Number of mipmap images equals 0");
		goto err;
	}
	
	if (VTF_VERSION (vtf, 7, 3)) {
		VtfResource rsrc;
		gboolean image_found = FALSE;
		gint i;
		
		for (i = 0; i < hdr.resource_count; i++) {
			memcpy (&rsrc, p, sizeof (VtfResource));
			p += sizeof (VtfResource);
			
			if (rsrc.type == VTF_RESOURCE_HIRES_IMAGE) {
				image_found = TRUE;
				p = data + rsrc.data;
				break;
			}
		}
		
		if (!image_found) {
			g_set_error (error, VTF_ERROR, VTF_ERROR_RESOURCE,
					"Could not find an image resoure");
			goto err;
		}
	} else {
		int offset = get_image_length (hdr.lowres_image_format,
				hdr.lowres_image_width, hdr.lowres_image_height) +
				hdr.header_size;
		p = data + offset;
	}
	
	g_ptr_array_set_size (vtf->images, 0);
	vtf->width = hdr.width;
	vtf->height = hdr.height;
	vtf->format = hdr.highres_image_format;
	
	/* skip all mipmaps */
	gint w, h, m, f;
	guint l;
	
	for (m = hdr.mipmap_count; m > 0; m--) {
		w = calc_mipmap_size (vtf->width, m);
		h = calc_mipmap_size (vtf->height, m);
		l = get_image_length (vtf->format, w, h);
		p += l * hdr.frames;
	}
	
	/* read our image */
	l = get_image_length (vtf->format, vtf->width, vtf->height);
	for (f = 0; f < hdr.frames; f++) {
		void *d = g_malloc (l);
		memcpy (d, p, l);
		g_ptr_array_add (vtf->images, d);
	}
	
	return vtf;
	
err:
	g_free (vtf);
	return NULL;
}


Vtf *vtf_open_fd (FILE *fd, GError **error)
{
	fseek (fd, 0L, SEEK_END);
	glong len = ftell (fd);
	fseek (fd, 0L, SEEK_SET);
	
	gpointer data = g_malloc (len);
	fread (data, len, 1, fd);
	Vtf *vtf = vtf_open_mem (data, len, error);
	g_free (data);
	
	return vtf;
}

Vtf *
vtf_open (const gchar *fname, GError **error)
{
	FILE *fd = g_fopen (fname, "rb");
	if (!fd) {
		g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
				"Could not open for reading: %s", g_strerror (errno));
		return NULL;
	}
	
	Vtf *vtf = vtf_open_fd (fd, error);
	
	fclose (fd);
	return vtf;
}

void
vtf_close (Vtf *vtf)
{
	g_ptr_array_free (vtf->images, TRUE);
	g_free (vtf);
}


guint
vtf_get_width (Vtf *vtf)
{
	return vtf->width;
}

guint
vtf_get_height (Vtf *vtf)
{
	return vtf->height;
}

guint
vtf_get_format (Vtf *vtf)
{
	return vtf->format;
}

guint
vtf_get_frame_count (Vtf *vtf)
{
	return vtf->images->len;
}


int
calc_mipmap_count (int width, int height)
{
	int size = MAX (width, height);
	int count = 1;
	
	while (size == 1) {
		size /= 2;
		count++;
	}
	
	return count;
}

void *
vtf_get_image (Vtf *vtf, int frame)
{
	return g_ptr_array_index (vtf->images, frame);
}

void *
vtf_get_image_rgba (Vtf *vtf, int frame)
{
	int length = get_image_length (VTF_FORMAT_RGBA8888, vtf->width, vtf->height);
	void *rgba_data = g_malloc (length);
	void *data = vtf_get_image (vtf, frame);
	
	switch (vtf->format) {
	case VTF_FORMAT_RGBA8888:
		memcpy (rgba_data, data, length);
		break;
	case VTF_FORMAT_DXT1:
		dxt1_to_rgba (rgba_data, vtf->width, vtf->height, data);
		break;
//	case VTF_FORMAT_DXT3:
//		dxt3_to_rgba (rgba_data, vtf->width, vtf->height, data);
//		break;
	case VTF_FORMAT_DXT5:
		dxt5_to_rgba (rgba_data, vtf->width, vtf->height, data);
		break;
	default:
		g_free (rgba_data);
		return NULL;
	}
	
	return rgba_data;
}
