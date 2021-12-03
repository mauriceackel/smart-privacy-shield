#pragma once

#include <gtk/gtk.h>
#include <sps/sps.h>

G_BEGIN_DECLS

#define SPS_TYPE_DIALOG_ADD_SOURCE (sps_dialog_add_source_get_type())
#define SPS_DIALOG_ADD_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_DIALOG_ADD_SOURCE, SpsDialogAddSource))
#define SPS_DIALOG_ADD_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_DIALOG_ADD_SOURCE, SpsDialogAddSource))
#define SPS_IS_DIALOG_ADD_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_DIALOG_ADD_SOURCE))
#define SPS_IS_DIALOG_ADD_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_DIALOG_ADD_SOURCE))

typedef struct _SpsDialogAddSource SpsDialogAddSource;
typedef struct _SpsDialogAddSourceClass SpsDialogAddSourceClass;

GType sps_dialog_add_source_get_type(void) G_GNUC_CONST;

struct _SpsDialogAddSource
{
    GtkDialog parent;

    GtkWidget *btnAdd, *btnAbort;
    GtkWidget *cbxSources;

    GArray *sourceInfos;
    GtkListStore *lstSources;
};

struct _SpsDialogAddSourceClass
{
    GtkDialogClass parent_class;
};

G_END_DECLS

SpsDialogAddSource *sps_dialog_add_source_new(GtkWindow *parent);

SpsSourceInfo *sps_dialog_add_source_run(SpsDialogAddSource *dialog);
