static GnomeVFSResult do_open           (GnomeVFSMethod *method,
                         GnomeVFSMethodHandle **method_handle,
                         GnomeVFSURI *uri,
                         GnomeVFSOpenMode mode,
                         GnomeVFSContext *context);
static GnomeVFSResult do_create         (GnomeVFSMethod *method,
                         GnomeVFSMethodHandle **method_handle,
                         GnomeVFSURI *uri,
                         GnomeVFSOpenMode mode,
                         gboolean exclusive,
                         guint perm,
                         GnomeVFSContext *context);
static GnomeVFSResult do_close          (GnomeVFSMethod *method,
                         GnomeVFSMethodHandle *method_handle,
                         GnomeVFSContext *context);
static GnomeVFSResult do_read       (GnomeVFSMethod *method,
                     GnomeVFSMethodHandle *method_handle,
                     gpointer buffer,
                     GnomeVFSFileSize num_bytes,
                     GnomeVFSFileSize *bytes_read,
                     GnomeVFSContext *context);
static GnomeVFSResult do_write          (GnomeVFSMethod *method,
                     GnomeVFSMethodHandle *method_handle,
                         gconstpointer buffer,
                         GnomeVFSFileSize num_bytes,
                         GnomeVFSFileSize *bytes_written,
                     GnomeVFSContext *context);
static GnomeVFSResult do_open_directory (GnomeVFSMethod *method,
                     GnomeVFSMethodHandle **method_handle,
                     GnomeVFSURI *uri,
                     GnomeVFSFileInfoOptions options,
                     GnomeVFSContext *context);
static GnomeVFSResult do_close_directory(GnomeVFSMethod *method,
                     GnomeVFSMethodHandle *method_handle,
                     GnomeVFSContext *context);
static GnomeVFSResult do_read_directory (GnomeVFSMethod *method,
                     GnomeVFSMethodHandle *method_handle,
                     GnomeVFSFileInfo *file_info,
                     GnomeVFSContext *context);
static GnomeVFSResult do_get_file_info  (GnomeVFSMethod *method,
                     GnomeVFSURI *uri,
                     GnomeVFSFileInfo *file_info,
                     GnomeVFSFileInfoOptions options,
                     GnomeVFSContext *context);
static GnomeVFSResult do_check_same_fs (GnomeVFSMethod *method,
										GnomeVFSURI *a,
										GnomeVFSURI *b,
										gboolean *same_fs_return,
										GnomeVFSContext *context);
static GnomeVFSResult do_get_file_info_from_handle (GnomeVFSMethod *method,
													GnomeVFSMethodHandle *method_handle,
													GnomeVFSFileInfo *file_info,
													GnomeVFSFileInfoOptions options,
													GnomeVFSContext *context);
static gboolean do_is_local (GnomeVFSMethod *method,
							 const GnomeVFSURI *uri);
