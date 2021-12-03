#pragma once

#include <gtk/gtk.h>
#include "../sourcedata.h"

G_BEGIN_DECLS

#define SPS_TYPE_SOURCE_ROW (sps_source_row_get_type())
#define SPS_SOURCE_ROW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_SOURCE_ROW, SpsSourceRow))
#define SPS_SOURCE_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_SOURCE_ROW, SpsSourceRow))
#define SPS_IS_SOURCE_ROW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_SOURCE_ROW))
#define SPS_IS_SOURCE_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_SOURCE_ROW))

typedef struct _SpsSourceRow SpsSourceRow;
typedef struct _SpsSourceRowClass SpsSourceRowClass;

GType sps_source_row_get_type(void) G_GNUC_CONST;

struct _SpsSourceRow
{
  GtkBox parent;

  SpsSourceData *sourceData;

  GtkWidget *lblName;
  GtkWidget *btnRemove;
  GtkWidget *btnEdit;
};

struct _SpsSourceRowClass
{
  GtkBoxClass parent_class;
};

G_END_DECLS

SpsSourceRow *sps_source_row_new(SpsSourceData *data);
