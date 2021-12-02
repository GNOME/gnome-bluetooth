#include <bluetooth-utils.c>

static void
test_appearance (void)
{
	g_assert_cmpint (bluetooth_appearance_to_type(0x03c2), ==, BLUETOOTH_TYPE_MOUSE);
}

int main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/bluetooth/appearance", test_appearance);

	return g_test_run ();
}
