#include <gtk/gtk.h>
#include "source-row.h"
#include "../sourcedata.h"
#include "main-window.h"
#include "dialog-edit-source.h"

G_DEFINE_TYPE(SpsSourceRow, sps_source_row, GTK_TYPE_BOX)

static void btn_edit_handler(GtkButton *btn, SpsSourceRow *row)
{
    GtkWindow *parentWindow = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn)));

    SpsDialogEditSource *dialog = sps_dialog_edit_source_new(parentWindow, row->sourceData);
    sps_dialog_edit_source_run(dialog);
}

static void btn_remove_handler(GtkButton *btn, SpsSourceRow *row)
{
    SpsApplication *app = SPS_APPLICATION(g_application_get_default());
    sps_application_remove_source(app, row->sourceData);
}

static void sps_source_row_init(SpsSourceRow *row)
{
    gtk_widget_init_template(GTK_WIDGET(row));
}

static void sps_source_row_finalize(GObject *object)
{
    SpsSourceRow *row = SPS_SOURCE_ROW(object);

    g_object_unref(row->sourceData);

    G_OBJECT_CLASS(sps_source_row_parent_class)->finalize(object);
}

static void sps_source_row_class_init(SpsSourceRowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->finalize = sps_source_row_finalize;

    // Load template from resources
    gtk_widget_class_set_template_from_resource(widget_class, "/sps/gui/source-row.glade");

    // Register elements
    gtk_widget_class_bind_template_child(widget_class, SpsSourceRow, lblName);
    gtk_widget_class_bind_template_child(widget_class, SpsSourceRow, btnRemove);
    gtk_widget_class_bind_template_child(widget_class, SpsSourceRow, btnEdit);

    // Register callbacks
    gtk_widget_class_bind_template_callback(widget_class, btn_remove_handler);
    gtk_widget_class_bind_template_callback(widget_class, btn_edit_handler);
}

SpsSourceRow *sps_source_row_new(SpsSourceData *sourceData)
{
    SpsSourceRow *row = g_object_new(SPS_TYPE_SOURCE_ROW, NULL);
    row->sourceData = g_object_ref(sourceData);

    return row;
}
