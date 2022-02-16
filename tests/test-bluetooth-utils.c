/*
 * Copyright (C) 2021 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <locale.h>
#include <bluetooth-utils.c>

static void
test_appearance (void)
{
	g_assert_cmpint (bluetooth_appearance_to_type(0x03c2), ==, BLUETOOTH_TYPE_MOUSE);
}

static void
test_class (void)
{
	g_assert_cmpint (bluetooth_class_to_type(268), ==, BLUETOOTH_TYPE_COMPUTER);
}

static void
test_type_to_string (void)
{
	GFlagsClass *fclass;
	guint i;

	setlocale (LC_ALL, "en_US.UTF-8");

	g_type_ensure (BLUETOOTH_TYPE_TYPE);
	fclass = G_FLAGS_CLASS (g_type_class_ref (BLUETOOTH_TYPE_TYPE));

	/* This verifies that all BluetoothType types have a description */
	g_assert_cmpstr (bluetooth_type_to_string (1 << 0), ==, "Unknown");
	for (i = 1; i < fclass->n_values; i++) {
		const char *desc = bluetooth_type_to_string (1 << i);

		if (g_strcmp0 (desc, "Unknown") == 0) {
			g_test_message ("Type %s returned description %s",
					fclass->values[i].value_name,
					desc);
			g_assert_cmpstr (desc, !=, "Unknown");
		}
	}
	g_assert_cmpint (_BLUETOOTH_TYPE_NUM_TYPES, ==, fclass->n_values);
}

static void
test_verify (void)
{
	struct {
		const char *bdaddr;
		gboolean valid;
	} addresses[] = {
		{ "11:22:33:44:55:66", TRUE },
		{ "11:22:33:44:55", FALSE },
		{ "11:22:33:44:55:FF", TRUE },
		{ "11:22:33:44:55:6", FALSE },
		{ "foobar", FALSE },
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS(addresses); i++) {
		if (addresses[i].valid)
			g_assert_true (bluetooth_verify_address (addresses[i].bdaddr));
		else
			g_assert_false (bluetooth_verify_address (addresses[i].bdaddr));
	}
}

int main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/bluetooth/appearance", test_appearance);
	g_test_add_func ("/bluetooth/class", test_class);
	g_test_add_func ("/bluetooth/verify", test_verify);
	g_test_add_func ("/bluetooth/type_to_string", test_type_to_string);

	return g_test_run ();
}
