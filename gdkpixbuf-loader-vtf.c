#include <gdk-pixbuf/gdk-pixbuf.h>
#include "vtf.h"


typedef struct _LoadContext {
	GByteArray *buffer;
	GdkPixbufModulePreparedFunc prepared;
	gpointer udata;
} LoadContext;


static GdkPixbuf *
gdk_pixbuf__vtf_image_load (FILE *fd, GError **error)
{
	Vtf *vtf = vtf_open_fd (fd, error);
	if (!vtf)
		return NULL;
	
	guchar *data = vtf_get_image_rgba (vtf, 0);
	gint width = vtf_get_width (vtf);
	gint height = vtf_get_height (vtf);
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB,
			TRUE, 8, width, height, width * 4, NULL, NULL);
	vtf_close (vtf);
	return pixbuf;
}


static gpointer
gdk_pixbuf__vtf_image_begin_load (GdkPixbufModuleSizeFunc size_func,
		GdkPixbufModulePreparedFunc prepare_func,
		GdkPixbufModuleUpdatedFunc update_func, gpointer udata, GError **error)
{
	LoadContext *lc = g_slice_new (LoadContext);
	lc->buffer = g_byte_array_sized_new (5 * 1024);
	lc->prepared = prepare_func;
	lc->udata = udata;
	return lc;
}


static gboolean
gdk_pixbuf__vtf_image_stop_load (gpointer context_ptr, GError **error)
{
	LoadContext *lc = context_ptr;
	
	Vtf *vtf = vtf_open_mem (lc->buffer->data, lc->buffer->len, error);
	if (!vtf)
		goto err;
	
	guchar *data = vtf_get_image_rgba (vtf, 0);
	if (!data) {
		g_set_error (error, 0, 0, "Format %s is not supported",
				vtf_get_format_name (vtf_get_format (vtf)));
		goto err;
	}
	
	gint width = vtf_get_width (vtf);
	gint height = vtf_get_height (vtf);
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB,
			TRUE, 8, width, height, width * 4, NULL, NULL);
	vtf_close (vtf);
	
	lc->prepared (pixbuf, NULL, lc->udata);
	
	g_byte_array_free (lc->buffer, TRUE);
	g_slice_free (LoadContext, lc);
	return TRUE;
	
err:
	g_byte_array_free (lc->buffer, TRUE);
	g_slice_free (LoadContext, lc);
	if (vtf)
		vtf_close (vtf);
	return FALSE;
}


static gboolean
gdk_pixbuf__vtf_image_load_increment (gpointer context_ptr, const guchar *data,
		guint size, GError **error)
{
	LoadContext *lc = context_ptr;
	g_byte_array_append (lc->buffer, data, size);
	return TRUE;
}



#ifndef INCLUDE_vtf
	#define MODULE_ENTRY(fn) G_MODULE_EXPORT void fn
#else
	#define MODULE_ENTRY(fn) void _gdk_pixbuf__vtf_##fn
#endif


MODULE_ENTRY (fill_vtable) (GdkPixbufModule* module)
{
	module->load = gdk_pixbuf__vtf_image_load;
	module->begin_load = gdk_pixbuf__vtf_image_begin_load;
	module->stop_load = gdk_pixbuf__vtf_image_stop_load;
    module->load_increment = gdk_pixbuf__vtf_image_load_increment;
}


MODULE_ENTRY (fill_info) (GdkPixbufFormat *info)
{
    static GdkPixbufModulePattern signature[] = {
    	{"VTF\0",	NULL,	100},
    	{NULL,		NULL,	0}
    };
    static gchar * mime_types[] = {
    	"image/x-vtf",
    	NULL
    };
    static gchar * extensions[] = {
    	"vtf",
    	NULL
    };
	
    info->name = "vtf";
    info->signature = signature;
    info->description = "Valve Texture Format";
    info->mime_types = mime_types;
    info->extensions = extensions;
    info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
    info->license = "LGPL";
}
