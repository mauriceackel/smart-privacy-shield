#include <gst/gst.h>
#include <gtk/gtk.h>
#include "sourcedata.h"

G_DEFINE_TYPE(SpsSourceData, sps_source_data, G_TYPE_OBJECT);

void sps_source_data_init(SpsSourceData *self)
{
    self->activePreprocessors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->activeDetectors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->activePostprocessors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    
    self->activePreprocessorElements = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->activeDetectorElements = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->activePostprocessorElements = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

void sps_source_data_finalize(GObject *object)
{
    SpsSourceData *self = SPS_SOURCE_DATA(object);

    g_hash_table_destroy(self->activePreprocessors);
    g_hash_table_destroy(self->activeDetectors);
    g_hash_table_destroy(self->activePostprocessors);

    g_hash_table_destroy(self->activePreprocessorElements);
    g_hash_table_destroy(self->activeDetectorElements);
    g_hash_table_destroy(self->activePostprocessorElements);

    sps_source_info_clear(&self->sourceInfo);

    G_OBJECT_CLASS(sps_source_data_parent_class)->finalize(object);
}

void sps_source_data_class_init(SpsSourceDataClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = sps_source_data_finalize;
}

SpsSourceData *sps_source_data_new()
{
    return g_object_new(SPS_TYPE_SOURCE_DATA, NULL);
}
