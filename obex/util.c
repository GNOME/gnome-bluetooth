/*
 * gnome-obex-server
 * Copyright (c) 2003-4 Edd Dumbill <edd@usefulinc.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "util.h"

static gchar *
newname(const gchar *str)
{
	gchar *rightparen;
	gchar *leftparen;
	gchar *res;
	gchar *suffix;
	gchar *work;

	work=g_strdup(str);
	res=NULL;
	suffix=g_strrstr(work, ".");
	if (suffix) {
		*suffix++=0;
	}
	rightparen=g_strrstr(work, ")");
	if (rightparen) {
		leftparen=g_strrstr(work, " (");
		if (leftparen && (rightparen-leftparen)>2) {
			unsigned int i;
			gboolean isnum=TRUE;
			gchar *c=g_strndup(&leftparen[2], 
					(rightparen-leftparen-2));
			for (i=0; i<strlen(c); i++) {
				if (!g_ascii_isdigit(c[i])) {
					isnum=FALSE;
					break;
				}
			}
			if (isnum) {
				int l=strtol(c, NULL, 10);
				if (l>=1) {
					l++;
					*leftparen=0;
					if (suffix) {
						res=g_strdup_printf("%s (%d).%s",
								work, l, suffix);
					} else {
						res=g_strdup_printf("%s (%d)",
								work, l);
					}
				}
			}
			g_free(c);
		}
	}
	if (res==NULL) {
		if (suffix) {
			res=g_strdup_printf("%s (1).%s", work, suffix);
		} else {
			res=g_strdup_printf("%s (1)", work);
		}
	}
	g_free(work);
	return res;	
}

gchar *
get_safe_unique_filename(char *origpath, 
						 const gchar *dir)
{
	gboolean done=FALSE;
	gchar *res=NULL;
	gchar *base=g_strdup(g_basename(origpath));

	while (!done) {
		res=g_build_filename(dir, base, NULL);
		if (g_file_test((const gchar*)res, G_FILE_TEST_EXISTS)) {
			gchar *newbase=newname(base);
			g_free(res);
			g_free(base);
			base=newbase;
		} else {
			done=TRUE;
		}
	}

	g_free(base);
	return res;
}

/* handy to keep around in case you need to verify
 * the above
void testnewname()
{
	printf("%s\n", newname("My File"));
	printf("%s\n", newname("My File ()"));
	printf("%s\n", newname("My File )1("));
	printf("%s\n", newname("My File (2)"));
	printf("%s\n", newname("My File (1) (2)"));
	printf("%s\n", newname("My File.(3)"));
	printf("%s\n", newname("My File (Second try)"));
	printf("%s\n", newname("fred.pdf"));
}
**/
