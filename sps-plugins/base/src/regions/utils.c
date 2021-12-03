#include <glib.h>
#include "utils.h"

typedef struct _ProcessingData ProcessingData;
struct _ProcessingData
{
    guint startIndex, endIndex;
    guint8 *data, *output;
};

static void convert(ProcessingData *pData)
{
    for (guint i = pData->startIndex; i < pData->endIndex; i++)
    {
        guint offset = i * 4;
        pData->output[offset] = pData->data[offset + 2];     // B -> R
        pData->output[offset + 1] = pData->data[offset + 1]; // G -> G
        pData->output[offset + 2] = pData->data[offset];     // R -> B
        pData->output[offset + 3] = pData->data[offset + 3]; // A -> A
    }

    g_free(pData);
}

guint8 *bgra2rgba(guint8 *data, guint size)
{
    guint8 *output = g_malloc(size);

    guint poolSize, chunkSize, pixelSize, startIndex, endIndex;
    poolSize = 4;
    pixelSize = size / 4;
    chunkSize = pixelSize / poolSize;

    startIndex = 0;
    endIndex = chunkSize;

    GThreadPool *pool = g_thread_pool_new((GFunc)convert, NULL, poolSize, FALSE, NULL);

    for (gint i = 0; i < poolSize; i++)
    {
        ProcessingData *pData = g_malloc(sizeof(ProcessingData));
        pData->data = data;
        pData->output = output;
        pData->startIndex = startIndex;
        pData->endIndex = i != poolSize - 1 ? endIndex : pixelSize;

        g_thread_pool_push(pool, pData, NULL);

        startIndex += chunkSize;
        endIndex += chunkSize;
    }

    g_thread_pool_free(pool, FALSE, TRUE);

    return output;
}
