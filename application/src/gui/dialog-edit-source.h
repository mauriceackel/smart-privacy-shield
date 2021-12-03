#pragma once

#include <gtk/gtk.h>
#include <sps/sps.h>
#include "../sourcedata.h"

G_BEGIN_DECLS

#define SPS_TYPE_DIALOG_EDIT_SOURCE (sps_dialog_edit_source_get_type())
#define SPS_DIALOG_EDIT_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_DIALOG_EDIT_SOURCE, SpsDialogEditSource))
#define SPS_DIALOG_EDIT_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_DIALOG_EDIT_SOURCE, SpsDialogEditSource))
#define SPS_IS_DIALOG_EDIT_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_DIALOG_EDIT_SOURCE))
#define SPS_IS_DIALOG_EDIT_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_DIALOG_EDIT_SOURCE))

typedef struct _SpsDialogEditSource SpsDialogEditSource;
typedef struct _SpsDialogEditSourceClass SpsDialogEditSourceClass;

GType sps_dialog_edit_source_get_type(void) G_GNUC_CONST;

struct _SpsDialogEditSource
{
    GtkDialog parent;

    SpsSourceData *sourceData;

    GtkWidget *btnAddPreprocessor, *btnRemovePreprocessor;
    GtkWidget *btnAddDetector, *btnRemoveDetector;
    GtkWidget *btnAddPostprocessor, *btnRemovePostprocessor;

    GtkMenu *menuAvailablePreprocessors, *menuAvailableDetectors, *menuAvailablePostprocessors;

    GtkWidget *tvActivePreprocessors, *tvActiveDetectors, *tvActivePostprocessors;
    GtkListStore *lstActivePreprocessors, *lstActiveDetectors, *lstActivePostprocessors;

    GtkWidget *vpDetails;
};

struct _SpsDialogEditSourceClass
{
    GtkDialogClass parent_class;
};

G_END_DECLS

SpsDialogEditSource *sps_dialog_edit_source_new(GtkWindow *parent, SpsSourceData *sourceData);

void sps_dialog_edit_source_run(SpsDialogEditSource *dialog);
