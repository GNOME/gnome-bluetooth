/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "bluetooth-hdy-column.h"

#include <glib/gi18n.h>
#include <math.h>

/*
 * SECTION:bluetooth-hdy-column
 * @short_description: A container letting its child grow up to a given width.
 * @Title: BluetoothHdyColumn
 *
 * The #BluetoothHdyColumn widget limits the size of the widget it contains to a
 * given maximum width. The expansion of the child from its minimum to its
 * maximum size is eased out for a smooth transition.
 *
 * If the child requires more than the requested maximum width, it will be
 * allocated the minimum width it can fit in instead.
 */

#define BLUETOOTH_EASE_OUT_TAN_CUBIC 3

enum {
  PROP_0,
  PROP_MAXIMUM_WIDTH,
  PROP_LINEAR_GROWTH_WIDTH,
  LAST_PROP,
};

struct _BluetoothHdyColumn
{
  GtkBin parent_instance;

  gint maximum_width;
  gint linear_growth_width;
};

static GParamSpec *props[LAST_PROP];

G_DEFINE_TYPE (BluetoothHdyColumn, bluetooth_hdy_column, GTK_TYPE_BIN)

static gdouble
ease_out_cubic (gdouble progress)
{
  gdouble tmp = progress - 1;

  return tmp * tmp * tmp + 1;
}

static void
bluetooth_hdy_column_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  BluetoothHdyColumn *self = BLUETOOTH_HDY_COLUMN (object);

  switch (prop_id) {
  case PROP_MAXIMUM_WIDTH:
    g_value_set_int (value, bluetooth_hdy_column_get_maximum_width (self));
    break;
  case PROP_LINEAR_GROWTH_WIDTH:
    g_value_set_int (value, bluetooth_hdy_column_get_linear_growth_width (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
bluetooth_hdy_column_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BluetoothHdyColumn *self = BLUETOOTH_HDY_COLUMN (object);

  switch (prop_id) {
  case PROP_MAXIMUM_WIDTH:
    bluetooth_hdy_column_set_maximum_width (self, g_value_get_int (value));
    break;
  case PROP_LINEAR_GROWTH_WIDTH:
    bluetooth_hdy_column_set_linear_growth_width (self, g_value_get_int (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static gint
get_child_width (BluetoothHdyColumn *self,
                 gint       width)
{
  GtkBin *bin = GTK_BIN (self);
  GtkWidget *child;
  gint minimum_width = 0, maximum_width;
  gdouble amplitude, threshold, progress;

  child = gtk_bin_get_child (bin);
  if (child == NULL)
    return 0;

  if (gtk_widget_get_visible (child))
    gtk_widget_get_preferred_width (child, &minimum_width, NULL);

  /* Sanitize the minimum width to use for computations. */
  minimum_width = MIN (MAX (minimum_width, self->linear_growth_width), self->maximum_width);

  if (width <= minimum_width)
    return width;

  /* Sanitize the maximum width to use for computations. */
  maximum_width = MAX (minimum_width, self->maximum_width);
  amplitude = maximum_width - minimum_width;
  threshold = (BLUETOOTH_EASE_OUT_TAN_CUBIC * amplitude + (gdouble) minimum_width);

  if (width >= threshold)
    return maximum_width;

  progress = (width - minimum_width) / (threshold - minimum_width);

  return ease_out_cubic (progress) * amplitude + minimum_width;
}

/* This private method is prefixed by the call name because it will be a virtual
 * method in GTK+ 4.
 */
static void
bluetooth_hdy_column_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;

  if (minimum)
    *minimum = 0;
  if (natural)
    *natural = 0;
  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;

  child = gtk_bin_get_child (bin);
  if (!(child && gtk_widget_get_visible (child)))
    return;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_get_preferred_width (child, minimum, natural);
  else {
    gint child_width = get_child_width (BLUETOOTH_HDY_COLUMN (widget), for_size);

    gtk_widget_get_preferred_height_and_baseline_for_width (child,
                                                            child_width,
                                                            minimum,
                                                            natural,
                                                            minimum_baseline,
                                                            natural_baseline);
  }
}

static void
bluetooth_hdy_column_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  bluetooth_hdy_column_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                      minimum, natural, NULL, NULL);
}

static void
bluetooth_hdy_column_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
                                                              gint       width,
                                                              gint      *minimum,
                                                              gint      *natural,
                                                              gint      *minimum_baseline,
                                                              gint      *natural_baseline)
{
  bluetooth_hdy_column_measure (widget, GTK_ORIENTATION_VERTICAL, width,
                      minimum, natural, minimum_baseline, natural_baseline);
}

static void
bluetooth_hdy_column_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  bluetooth_hdy_column_measure (widget, GTK_ORIENTATION_VERTICAL, -1,
                      minimum, natural, NULL, NULL);
}

static void
bluetooth_hdy_column_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  BluetoothHdyColumn *self = BLUETOOTH_HDY_COLUMN (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation child_allocation;
  gint baseline;
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);

  child = gtk_bin_get_child (bin);
  if (child == NULL)
    return;

  child_allocation.width = get_child_width (self, allocation->width);
  child_allocation.height = allocation->height;

  if (!gtk_widget_get_has_window (widget)) {
    /* This allways center the child vertically. */
    child_allocation.x = allocation->x + (allocation->width - child_allocation.width) / 2;
    child_allocation.y = allocation->y;
  }
  else {
    child_allocation.x = 0;
    child_allocation.y = 0;
  }

  baseline = gtk_widget_get_allocated_baseline (widget);
  gtk_widget_size_allocate_with_baseline (child, &child_allocation, baseline);
}

static void
bluetooth_hdy_column_class_init (BluetoothHdyColumnClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = bluetooth_hdy_column_get_property;
  object_class->set_property = bluetooth_hdy_column_set_property;

  widget_class->get_preferred_width = bluetooth_hdy_column_get_preferred_width;
  widget_class->get_preferred_height = bluetooth_hdy_column_get_preferred_height;
  widget_class->get_preferred_height_and_baseline_for_width = bluetooth_hdy_column_get_preferred_height_and_baseline_for_width;
  widget_class->size_allocate = bluetooth_hdy_column_size_allocate;

  gtk_container_class_handle_border_width (container_class);

  /**
   * BluetoothHdyColumn:maximum_width:
   *
   * The maximum width to allocate to the child.
   */
  props[PROP_MAXIMUM_WIDTH] =
      g_param_spec_int ("maximum-width",
                        _("Maximum width"),
                        _("The maximum width allocated to the child"),
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * BluetoothHdyColumn:linear_growth_width:
   *
   * The width up to which the child will be allocated all the width.
   */
  props[PROP_LINEAR_GROWTH_WIDTH] =
      g_param_spec_int ("linear-growth-width",
                        _("Linear growth width"),
                        _("The width up to which the child will be allocated all the width"),
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, "bluetoothcolumn");
}

static void
bluetooth_hdy_column_init (BluetoothHdyColumn *self)
{
}

/**
 * bluetooth_hdy_column_new:
 *
 * Creates a new #BluetoothHdyColumn.
 *
 * Returns: a new #BluetoothHdyColumn
 */
BluetoothHdyColumn *
bluetooth_hdy_column_new (void)
{
  return g_object_new (BLUETOOTH_TYPE_HDY_COLUMN, NULL);
}

/**
 * bluetooth_hdy_column_get_maximum_width:
 * @self: a #BluetoothHdyColumn
 *
 * Gets the maximum width to allocate to the contained child.
 *
 * Returns: the maximum width to allocate to the contained child.
 */
gint
bluetooth_hdy_column_get_maximum_width (BluetoothHdyColumn *self)
{
  g_return_val_if_fail (BLUETOOTH_IS_HDY_COLUMN (self), 0);

  return self->maximum_width;
}

/**
 * bluetooth_hdy_column_set_maximum_width:
 * @self: a #BluetoothHdyColumn
 * @maximum_width: the maximum width
 *
 * Sets the maximum width to allocate to the contained child.
 */
void
bluetooth_hdy_column_set_maximum_width (BluetoothHdyColumn *self,
                                    gint             maximum_width)
{
  g_return_if_fail (BLUETOOTH_IS_HDY_COLUMN (self));

  self->maximum_width = maximum_width;
}

/**
 * bluetooth_hdy_column_get_linear_growth_width:
 * @self: a #BluetoothHdyColumn
 *
 * Gets the width up to which the child will be allocated all the available
 * width and starting from which it will be allocated a portion of the available
 * width. In bith cases the allocated width won't exceed the declared maximum.
 *
 * Returns: the width up to which the child will be allocated all the available
 * width.
 */
gint
bluetooth_hdy_column_get_linear_growth_width (BluetoothHdyColumn *self)
{
  g_return_val_if_fail (BLUETOOTH_IS_HDY_COLUMN (self), 0);

  return self->linear_growth_width;
}

/**
 * bluetooth_hdy_column_set_linear_growth_width:
 * @self: a #BluetoothHdyColumn
 * @linear_growth_width: the linear growth width
 *
 * Sets the width up to which the child will be allocated all the available
 * width and starting from which it will be allocated a portion of the available
 * width. In bith cases the allocated width won't exceed the declared maximum.
 *
 */
void
bluetooth_hdy_column_set_linear_growth_width (BluetoothHdyColumn *self,
                                          gint             linear_growth_width)
{
  g_return_if_fail (BLUETOOTH_IS_HDY_COLUMN (self));

  self->linear_growth_width = linear_growth_width;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}
