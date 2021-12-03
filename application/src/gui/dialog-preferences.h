#pragma once

#include <gtk/gtk.h>
#include <sps/sps.h>
#include "../sourcedata.h"

G_BEGIN_DECLS

#define SPS_TYPE_DIALOG_PREFERENCES (sps_dialog_preferences_get_type())
#define SPS_DIALOG_PREFERENCES(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SPS_TYPE_DIALOG_PREFERENCES, SpsDialogPreferences))
#define SPS_DIALOG_PREFERENCES_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), SPS_TYPE_DIALOG_PREFERENCES, SpsDialogPreferences))
#define SPS_IS_DIALOG_PREFERENCES(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPS_TYPE_DIALOG_PREFERENCES))
#define SPS_IS_DIALOG_PREFERENCES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPS_TYPE_DIALOG_PREFERENCES))

typedef struct _SpsDialogPreferences SpsDialogPreferences;
typedef struct _SpsDialogPreferencesClass SpsDialogPreferencesClass;

GType sps_dialog_preferences_get_type(void) G_GNUC_CONST;

struct _SpsDialogPreferences
{
    GtkDialog parent;

    SpsRegistry *registry;

    GtkWidget *btnAddPreprocessor, *btnRemovePreprocessor;
    GtkWidget *btnAddDetector, *btnRemoveDetector;
    GtkWidget *btnAddPostprocessor, *btnRemovePostprocessor;

    GtkMenu *menuAvailablePreprocessors, *menuAvailableDetectors, *menuAvailablePostprocessors;

    GtkWidget *tvActivePreprocessors, *tvActiveDetectors, *tvActivePostprocessors;
    GtkListStore *lstActivePreprocessors, *lstActiveDetectors, *lstActivePostprocessors;
};

struct _SpsDialogPreferencesClass
{
    GtkDialogClass parent_class;
};

G_END_DECLS

SpsDialogPreferences *sps_dialog_preferences_new(GtkWindow *parent);

void sps_dialog_preferences_run(SpsDialogPreferences *dialog);
