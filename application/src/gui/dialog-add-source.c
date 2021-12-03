#include <gtk/gtk.h>
#include <sps/sps.h>
#include "dialog-add-source.h"

G_DEFINE_TYPE(SpsDialogAddSource, sps_dialog_add_source, GTK_TYPE_DIALOG)

static void cbx_sources_changed(GtkComboBox *cbx, SpsDialogAddSource *dialog)
{
    gint selectedIndex = gtk_combo_box_get_active(cbx);

    gtk_widget_set_sensitive(GTK_WIDGET(dialog->btnAdd), selectedIndex != -1);
}

static void sps_dialog_add_source_load_sources(SpsDialogAddSource *dialog)
{
    // Get source info
    sps_sources_list(dialog->sourceInfos);

    // Fill dropdown with sources
    GtkTreeIter iter;
    for (gint i = 0; i < dialog->sourceInfos->len; i++)
    {
        SpsSourceInfo *info = &g_array_index(dialog->sourceInfos, SpsSourceInfo, i);
        gtk_list_store_append(dialog->lstSources, &iter);
        gtk_list_store_set(dialog->lstSources, &iter, 0, info->name, -1);
    }
}

static void sps_dialog_add_source_init(SpsDialogAddSource *dialog)
{
    gtk_widget_init_template(GTK_WIDGET(dialog));

    dialog->sourceInfos = g_array_new(FALSE, FALSE, sizeof(SpsSourceInfo));
    g_array_set_clear_func(dialog->sourceInfos, sps_source_info_clear);

    sps_dialog_add_source_load_sources(dialog);
}

static void sps_dialog_add_source_finalize(GObject *object)
{
    SpsDialogAddSource *dialog = SPS_DIALOG_ADD_SOURCE(object);

    g_array_free(dialog->sourceInfos, TRUE);

    G_OBJECT_CLASS(sps_dialog_add_source_parent_class)->finalize(object);
}

static void sps_dialog_add_source_class_init(SpsDialogAddSourceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->finalize = sps_dialog_add_source_finalize;

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/gui/dialog-add-source.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsDialogAddSource, btnAdd);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogAddSource, btnAbort);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogAddSource, cbxSources);
    gtk_widget_class_bind_template_child(widget_class, SpsDialogAddSource, lstSources);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, cbx_sources_changed);
}

SpsDialogAddSource *sps_dialog_add_source_new(GtkWindow *parent)
{
    return g_object_new(SPS_TYPE_DIALOG_ADD_SOURCE, "transient-for", parent, NULL);
}

SpsSourceInfo *sps_dialog_add_source_run(SpsDialogAddSource *dialog)
{
    GtkResponseType result;
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(GTK_WIDGET(dialog));

    if (result != GTK_RESPONSE_OK)
        return NULL;

    gint selectedIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(dialog->cbxSources));
    if (selectedIndex == -1)
        return NULL;

    // Result will live until the dialog object is finalized
    return &g_array_index(dialog->sourceInfos, SpsSourceInfo, selectedIndex);
}
