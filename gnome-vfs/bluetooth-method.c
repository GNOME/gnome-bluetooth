#include <errno.h>
#include <libgnomevfs/gnome-vfs-cancellation.h>
#include <libgnomevfs/gnome-vfs-context.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-module-shared.h>
#include <libgnomevfs/gnome-vfs-module.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-parse-ls.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libbonobo.h>
#include <unistd.h>

#include "bluetooth-method.h"

#include "GNOME_Bluetooth_Manager.h"

#define DESKTOP_FORMAT_LINK "[Desktop Entry]\nVersion=1.0\nEncoding=UTF-8\nType=Application\nName=%s\nIcon=%s\nExec=gnome-obex-send --dest %s %%F\nX-Bluetooth-Address=%s\n"

#define DESKTOP_FORMAT_APP  "[Desktop Entry]\nVersion=1.0\nEncoding=UTF-8\nType=Link\nName=%s\nIcon=%s\nURL=bluetooth:///%s/stats\nX-Bluetooth-Address=%s\n"

#define GET_MAJOR_CLASS(t) ((t>>8)&0x1f)

static gchar *icon_names[]={
  "btdevice-misc.png",            /* MISC */
  "btdevice-computer.png",        /* COMPUTER */
  "btdevice-phone.png",           /* PHONE */
  "btdevice-lan.png",             /* LAN */
  "btdevice-audiovideo.png",      /* AUDIOVIDEO */
  "btdevice-peripheral.png",      /* PERIPHERAL */
  "btdevice-imaging.png"          /* IMAGING */
};

static GnomeVFSMethod method = {
  sizeof (GnomeVFSMethod),
  do_open,
  do_create, /* create */
  do_close,
  do_read, /* read */
  do_write, /* write */
  NULL, /* seek */
  NULL, /* tell */
  NULL, /* truncate */
  do_open_directory,
  do_close_directory,
  do_read_directory,
  do_get_file_info,
  do_get_file_info_from_handle, /* get_file_info_from_handle */
  do_is_local,
  NULL, /* make directory */
  NULL, /* remove directory */
  NULL, /* move directory */
  NULL, /* unlink */
  do_check_same_fs, /* check_same_fs */
  NULL, /* set_file_info */
  NULL, /* truncate */
  NULL, /* find_directory */
  NULL, /* create_symbolic_link */
  NULL, /* do_monitor_add */
  NULL, /* do_monitor_cancel */
  NULL  /* do_file_control */
};

static GMutex *btmanager_lock = NULL;
static gboolean did_we_bonobo_init=FALSE;
static GNOME_Bluetooth_Manager btmanager;

typedef struct {
  GNOME_Bluetooth_DeviceList *list;
  guint listptr;
} BluetoothContext;

typedef struct {
  GNOME_Bluetooth_Device *dev;
  gchar *contents;
  guint len;
  guint ptr;
  gboolean writing;
} BluetoothDeviceContext;

#define DEBUG_ENABLE


#ifdef DEBUG_ENABLE
#define DEBUG_PRINT(x) g_print x
#else
#define DEBUG_PRINT(x)
#endif

static GnomeVFSResult
check_for_bdaddr_from_desktop_file(BluetoothDeviceContext **tdc,
								   GnomeVFSURI *uri)
{
    GnomeVFSResult result = GNOME_VFS_OK;
	CORBA_Environment ev;
	BluetoothDeviceContext *dc;
	gchar *bdaddr;
	gchar *path;
	gchar *right;


	CORBA_exception_init(&ev);
	path=g_strdup(gnome_vfs_uri_extract_short_path_name(uri));
    DEBUG_PRINT (("in bdaddr_from_desktop, path %s\n", path));

	/* we assume the bdaddr is to be
	   found to the left of .desktop
	*/
	right=g_strrstr(path, ".desktop");
	if (right) {
	  *right=0; /* make it end at .desktop */
	}

	/* now to get those colons back! */
	bdaddr=gnome_vfs_unescape_string(path, "");
	g_free(path);
	DEBUG_PRINT (("bdaddr is %s\n", bdaddr));

	*tdc=NULL;

    result = GNOME_VFS_OK;

	dc=g_new0(BluetoothDeviceContext, 1);
	dc->dev=NULL;

	g_mutex_lock(btmanager_lock);
	GNOME_Bluetooth_Manager_getDeviceInfo(btmanager,
										  &dc->dev,
										  bdaddr,
										  &ev);
	g_mutex_unlock(btmanager_lock);

	if (BONOBO_EX(&ev)) {
	  gchar *err=bonobo_exception_get_text (&ev);
	  g_warning("bluetooth-method: exception '%s'", err);
	  CORBA_free(dc->dev);
	  g_free(err);
	  g_free(dc);
	  result = GNOME_VFS_ERROR_INTERNAL;
	} else {
	  /* everything's ok */
	  *tdc=dc;
	}

	CORBA_exception_free(&ev);
	return result;
}

static GnomeVFSResult
do_open (GnomeVFSMethod *method,
     GnomeVFSMethodHandle **method_handle,
     GnomeVFSURI *uri,
     GnomeVFSOpenMode mode,
     GnomeVFSContext *context)
{
    GnomeVFSResult result = GNOME_VFS_OK;
	BluetoothDeviceContext *dc;

    DEBUG_PRINT (("in do_open, uri %s\n", uri->text));

	*method_handle=NULL;

    if (mode == GNOME_VFS_OPEN_READ) {
	  result = check_for_bdaddr_from_desktop_file(&dc, uri);

	  if (result == GNOME_VFS_OK) {
		/* everything's ok */
		gchar *encbdaddr;
		gchar *iconpath;
		guint cls;
		encbdaddr=gnome_vfs_escape_string(dc->dev->bdaddr);

		cls=GET_MAJOR_CLASS(dc->dev->deviceclass);

		if (cls>GNOME_Bluetooth_MAJOR_IMAGING)
		  cls=GNOME_Bluetooth_MAJOR_MISC;

		iconpath=g_strdup_printf("%s/pixmaps/%s",
								 DATADIR,
								 icon_names[cls]);

		*method_handle=(GnomeVFSMethodHandle *)dc;
		if (g_strrstr(gnome_vfs_uri_extract_short_path_name(uri),
					  ".desktop")) {

		  dc->contents=g_strdup_printf
			(DESKTOP_FORMAT_LINK,
			 dc->dev->name,
			 iconpath,
			 dc->dev->bdaddr,
			 dc->dev->bdaddr);

		} else {
		  // redundant but left in in case i ever
		  // change my mind about how this is meant to work
		  dc->contents=g_strdup_printf
			(DESKTOP_FORMAT_APP,
			 dc->dev->name,
			 DATADIR "/pixmaps/blueradio-48.png",
			 encbdaddr,
			 dc->dev->bdaddr);
		}
		g_free(encbdaddr);
		g_free(iconpath);
		DEBUG_PRINT (("%s", dc->contents ));
		dc->len=strlen(dc->contents);
		dc->ptr=0;
	  }

    } else if (mode == GNOME_VFS_OPEN_WRITE) {
	  result = GNOME_VFS_ERROR_INVALID_OPEN_MODE;
    } else {
	  result = GNOME_VFS_ERROR_INVALID_OPEN_MODE;
    }

    return result;
}

static GnomeVFSResult
do_create (GnomeVFSMethod *method,
       GnomeVFSMethodHandle **method_handle,
       GnomeVFSURI *uri,
       GnomeVFSOpenMode mode,
       gboolean exclusive,
       guint perm,
       GnomeVFSContext *context)
{
    GnomeVFSResult result;
 	GnomeVFSURI *parent;

	if (mode != GNOME_VFS_OPEN_WRITE) {
	  return GNOME_VFS_ERROR_INVALID_OPEN_MODE;
    }

	parent = gnome_vfs_uri_get_parent (uri);

	*method_handle = NULL;

    DEBUG_PRINT (("in do_create: uri is %s parent %p\n", uri->text,
			  parent));

   	if (parent == NULL) {
	  result = GNOME_VFS_ERROR_READ_ONLY;
	} else {

	  DEBUG_PRINT (("parent uri %s\n", parent->text));

	  if (strcmp (gnome_vfs_uri_get_path(parent),
				  GNOME_VFS_URI_PATH_STR) == 0) {
		// can't create anything in the root
		result = GNOME_VFS_ERROR_READ_ONLY;
	  } else {
		// if the parent is a valid
		// bdaddr .desktop file we'll permit creation
		BluetoothDeviceContext *dc;
		result = check_for_bdaddr_from_desktop_file(&dc, parent);

		if (result == GNOME_VFS_OK) {
		  /*
			 we're allowing this file creation: it will result
			 in the attempted send of a file to a remote device
			 via OBEX push

			 if we decide to go ahead, we'll write the file
			 contents to a temp location, and then do the OBEX
			 push synchronously
		  */
		  result = GNOME_VFS_ERROR_READ_ONLY;
		  DEBUG_PRINT (("sending file to %s\n", dc->dev->bdaddr));

		  /* register that we're writing to OBEX */
		  dc->writing=TRUE;

		  /* dc and dc->dev are freed when do_close is called */

		  /* NB. this is unused as i couldn't persuade nautilus
			 to do what i want.  for now we rely on the d&d semantics
			 of an Application .desktop link */
		} else {
		  // can't find the path we're meant to be writing to
		  result = GNOME_VFS_ERROR_READ_ONLY;
		}
	  }
	  gnome_vfs_uri_unref (parent);
    } /* endif parent == NULL */

    return result;
}

static GnomeVFSResult
do_close (GnomeVFSMethod *method,
      GnomeVFSMethodHandle *method_handle,
      GnomeVFSContext *context)
{
    GnomeVFSResult result;
    DEBUG_PRINT (("in do_close\n"));

	result=GNOME_VFS_OK;

	if (method_handle) {
	  BluetoothDeviceContext *dc=(BluetoothDeviceContext*)method_handle;
	  CORBA_free(dc->dev);
	  if (dc->writing) {
		/* we've been creating a file and sending it via
		   OBEX */
	  } else {
		/* we've been reading the .desktop file */
		if (dc->contents)
		  g_free(dc->contents);
	  }
	  g_free(dc);
	}

    return result;
}

static GnomeVFSResult
do_read (GnomeVFSMethod *method,
     GnomeVFSMethodHandle *method_handle,
     gpointer buffer,
     GnomeVFSFileSize num_bytes,
     GnomeVFSFileSize *bytes_read,
     GnomeVFSContext *context)
{
    GnomeVFSResult result;
	BluetoothDeviceContext *dc=(BluetoothDeviceContext*)method_handle;

    DEBUG_PRINT (("in do_read\n"));

	if (dc->ptr<dc->len) {
	  GnomeVFSFileSize n;
	  if (num_bytes>dc->len)
		n=dc->len;
	  else
		n=num_bytes;
	  memcpy(buffer, dc->contents, n);
	  dc->ptr+=n;
	  *bytes_read=n;
	  result = GNOME_VFS_OK;
	} else {
	  result = GNOME_VFS_ERROR_EOF;
	}

    return result;
}

static GnomeVFSResult
do_write (GnomeVFSMethod *method,
      GnomeVFSMethodHandle *method_handle,
      gconstpointer buffer,
      GnomeVFSFileSize num_bytes,
      GnomeVFSFileSize *bytes_written,
      GnomeVFSContext *context)
{
    GnomeVFSResult result;
    DEBUG_PRINT (("in do_write\n"));
    result = GNOME_VFS_ERROR_NOT_SUPPORTED;
    return result;
}

static GnomeVFSResult
do_open_directory (GnomeVFSMethod *method,
	   GnomeVFSMethodHandle **method_handle,
	   GnomeVFSURI *uri,
	   GnomeVFSFileInfoOptions options,
	   GnomeVFSContext *context)
{
    GnomeVFSResult result;
	CORBA_Environment ev;
	BluetoothContext *bc=g_new0(BluetoothContext, 1);

	CORBA_exception_init(&ev);
    DEBUG_PRINT (("in do_open_directory\n"));

	if (bc) {
	  g_mutex_lock(btmanager_lock);
	  GNOME_Bluetooth_Manager_knownDevices(btmanager, &bc->list, &ev);
	  g_mutex_unlock(btmanager_lock);

	  if (BONOBO_EX (&ev)){
		gchar *err=bonobo_exception_get_text (&ev);
		g_warning("bluetooth-method: exception '%s'", err);
		g_free(err);

		result = GNOME_VFS_ERROR_INTERNAL;
	  } else {
		bc->listptr=0;
	  }
	} else {
	  g_warning("Couldn't allocate memory for BluetoothContext");
	  result = GNOME_VFS_ERROR_INTERNAL;
	}

	*method_handle = (GnomeVFSMethodHandle*)bc;

    result = GNOME_VFS_OK;

	CORBA_exception_free(&ev);
	return result;
}

static GnomeVFSResult
do_close_directory (GnomeVFSMethod *method,
	    GnomeVFSMethodHandle *method_handle,
	    GnomeVFSContext *context)
{
    GnomeVFSResult result;
	BluetoothContext *bc=(BluetoothContext*)method_handle;

    DEBUG_PRINT (("in do_close_directory\n"));

	CORBA_free(bc->list);
	g_free(bc);
    result = GNOME_VFS_OK;
    return result;
}

static void
fill_in_file_info(GnomeVFSFileInfo *file_info, gchar *bdaddr,
				  gchar *suffix)
{
  file_info->type=GNOME_VFS_FILE_TYPE_REGULAR;
  file_info->valid_fields =
	GNOME_VFS_FILE_INFO_FIELDS_TYPE |
	GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
  file_info->name=g_strdup_printf("%s%s", bdaddr, suffix);
  file_info->mime_type=g_strdup("application/x-gnome-app-info");
}

static GnomeVFSResult
do_read_directory (GnomeVFSMethod *method,
				   GnomeVFSMethodHandle *method_handle,
				   GnomeVFSFileInfo *file_info,
				   GnomeVFSContext *context)
{
  GnomeVFSResult result = GNOME_VFS_OK;
  BluetoothContext *bc=(BluetoothContext*)method_handle;

  DEBUG_PRINT (("in do_read_directory\n"));

  if (bc->list && bc->listptr < bc->list->_length) {
	fill_in_file_info(file_info,
					  bc->list->_buffer[bc->listptr].bdaddr,
					  ".desktop");
	bc->listptr++;
  } else {
	result = GNOME_VFS_ERROR_EOF;
  }

  return result;
}

GnomeVFSResult
do_get_file_info (GnomeVFSMethod *method,
	  GnomeVFSURI *uri,
		  GnomeVFSFileInfo *file_info,
		  GnomeVFSFileInfoOptions options,
	  GnomeVFSContext *context)
{
    GnomeVFSResult result = GNOME_VFS_OK;
    GnomeVFSURI *parent = gnome_vfs_uri_get_parent (uri);
    DEBUG_PRINT (("in do_get_file_info: uri is %s parent %p\n", uri->text,
			  parent));

	if (parent == NULL) {
	  // we're the root, so fake up something nice
	  // and pretty
	  file_info->name = g_strdup("/");
	  file_info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
	  file_info->mime_type = g_strdup ("x-directory/normal");
	  file_info->valid_fields = GNOME_VFS_FILE_INFO_FIELDS_TYPE |
		GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
	  result = GNOME_VFS_OK;

	} else {

	  DEBUG_PRINT (("parent uri %s\n", parent->text));

	  if (strcmp (gnome_vfs_uri_get_path(parent),
				  GNOME_VFS_URI_PATH_STR) == 0) {
		// looking for something in the root, so if it's a valid
		// bdaddr .desktop file we'll fill in the info
		BluetoothDeviceContext *dc;
		result = check_for_bdaddr_from_desktop_file(&dc, uri);

		if (result == GNOME_VFS_OK) {
		  /* fill in file info */
		  if (g_strrstr(gnome_vfs_uri_extract_short_path_name(uri),
						".desktop")) {
			fill_in_file_info(file_info, dc->dev->bdaddr, ".desktop");
		  } else {
			fill_in_file_info(file_info, dc->dev->bdaddr, "");
		  }

		  result = GNOME_VFS_OK;
		  CORBA_free(dc->dev);
		  g_free(dc);

		} else {
		  result = GNOME_VFS_ERROR_NOT_FOUND;
		}

	  } else {
		// must be looking for a subdirectory of one of the bluetooth
		// devices, these don't exist.

		result = GNOME_VFS_ERROR_NOT_FOUND;
	  }
	  gnome_vfs_uri_unref (parent);
    }

    return result;
}


static GnomeVFSResult
do_get_file_info_from_handle (GnomeVFSMethod *method,
		  GnomeVFSMethodHandle *method_handle,
		  GnomeVFSFileInfo *file_info,
		  GnomeVFSFileInfoOptions options,
		  GnomeVFSContext *context)
{
    GnomeVFSResult result;
    DEBUG_PRINT (("do_get_file_info_from_handle: %p\n", method_handle));
    result = GNOME_VFS_OK;
    return result;
}


gboolean
do_is_local (GnomeVFSMethod *method, const GnomeVFSURI *uri)
{
	return TRUE;
}

static GnomeVFSResult
do_check_same_fs (GnomeVFSMethod *method,
	GnomeVFSURI *a,
	GnomeVFSURI *b,
	gboolean *same_fs_return,
	GnomeVFSContext *context)
{
    DEBUG_PRINT (("in do_check_same_fs\n"));
	*same_fs_return=FALSE;
    return GNOME_VFS_OK;
}

GnomeVFSMethod *
vfs_module_init (const char *method_name, const char *args)
{
  int argc=0;
  DEBUG_PRINT (("in vfs_module_init\n"));
  if (!bonobo_is_initialized()) {
	did_we_bonobo_init=TRUE;
	if (!bonobo_init(&argc, NULL))
	  g_error("Couldn't initialize bonobo");
  }
  bonobo_activate();
  btmanager=bonobo_get_object("OAFIID:GNOME_Bluetooth_Manager",
			      "GNOME/Bluetooth/Manager", NULL);

  if (btmanager==CORBA_OBJECT_NIL) {
    g_error("Couldn't get instance of BT Manager");
	if (did_we_bonobo_init)
	  bonobo_debug_shutdown();
  }

  btmanager_lock = g_mutex_new();

  return &method;
}

void
vfs_module_shutdown (GnomeVFSMethod *method)
{
  DEBUG_PRINT (("in vfs_module_shutdown\n"));
  bonobo_object_release_unref (btmanager, NULL);
  g_mutex_free (btmanager_lock);
}
