#ifndef _MOBLIN_PANEL_H
#define _MOBLIN_PANEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MOBLIN_TYPE_PANEL moblin_panel_get_type()

#define MOBLIN_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  MOBLIN_TYPE_PANEL, MoblinPanel))

#define MOBLIN_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  MOBLIN_TYPE_PANEL, MoblinPanelClass))

#define MOBLIN_IS_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  MOBLIN_TYPE_PANEL))

#define MOBLIN_IS_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  MOBLIN_TYPE_PANEL))

#define MOBLIN_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  MOBLIN_TYPE_PANEL, MoblinPanelClass))

typedef struct _MoblinPanel MoblinPanel;
typedef struct _MoblinPanelClass MoblinPanelClass;
typedef struct _MoblinPanelPrivate MoblinPanelPrivate;

struct _MoblinPanel
{
  GtkHBox parent;
};

struct _MoblinPanelClass
{
  GtkHBoxClass parent_class;
  MoblinPanelPrivate *priv;
};

GType moblin_panel_get_type (void);

GtkWidget *moblin_panel_new (void);

G_END_DECLS

#endif /* _MOBLIN_PANEL_H */
