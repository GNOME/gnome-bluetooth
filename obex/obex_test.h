#ifndef OBEX_TEST_H
#define OBEX_TEST_H

struct context
{
  int serverdone;
  int clientdone;
  char *get_name;	/* Name of last get-request */
  void *app;
  const char *save_dir;
  int to_send;
  int sent;
  void (*update)(void *);
  int fileDesc;
  int error;
  int usercancel;
};

#endif
