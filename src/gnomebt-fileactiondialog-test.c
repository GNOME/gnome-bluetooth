
#include <stdlib.h>

#include "gnomebt-fileactiondialog.h"

static const gchar *fnames[] = {
	"/tmp/picture.jpg",
	"/etc/motd",
	"/tmp/vmlinuz",
	"/tmp/README",
	NULL
};

int main(int argc, char **argv)
{
	GnomebtFileActionDialog *dlg;
	gint result;
	gint i = 0;
	gboolean bad = FALSE;
	
	gtk_init (&argc, &argv);

	while (fnames[i]) {
		if (g_file_test (fnames[i], G_FILE_TEST_EXISTS) == FALSE) {
			g_warning ("%s doesn't exist", fnames[i]);
			bad = TRUE;
		}
		i++;
	}
	if (bad != FALSE) {
		g_warning ("Please fix the errors above before continuing");
		return 1;
	}

	i = 0;
	while (fnames [i]) {
	
		dlg = gnomebt_fileactiondialog_new (
				"Edd's phone", fnames[i]);
		result = gtk_dialog_run (GTK_DIALOG (dlg));
		switch (result) {
			case GNOMEBT_FILEACTION_DELETE:
				g_message ("user said delete");
				break;
			case GNOMEBT_FILEACTION_CLOSE:
				g_message ("user said do nothing");
				break;
			case GNOMEBT_FILEACTION_OPEN:
				g_message ("user said open");
				gnomebt_fileactiondialog_open (dlg);
				break;
		}
	
		gtk_widget_destroy(GTK_WIDGET(dlg));

		i++;
	}

	return 0;
}

