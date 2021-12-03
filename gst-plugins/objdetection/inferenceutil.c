#include "inferenceutil.h"
#include "inferencedata.h"
#include "gstobjdetection.h"
// TODO: Find a way to run yolo on the GPU platform independently

#include <gst/gst.h>
#include <gst/video/video.h>
#include <onnxruntime_c_api.h>
#include <detectionmeta.h>
#ifdef WIN32
#include <Windows.h>
#include <gmodule.h>
#endif
#include <stdio.h>
#include <math.h>

#define INPUT_CHANNELS 4 // Amount of channels in the raw input frame (BGRA)
#define MODEL_CHANNELS 3 // Amount of channels in the model input frame (BGR)
// #define MODEL_SIZE MODEL_WIDTH *MODEL_HEIGHT                                                              // Amount of pixels in the model input frame
#define MODEL_SIZE(modelProportion) modelProportion *modelProportion                                                                                                          // Amount of pixels in the model input frame
#define EXPECTED_INPUT_SIZE(modelProportion) MODEL_SIZE(modelProportion) * INPUT_CHANNELS                                                                                     // Amount of bytes in raw input
#define EXPECTED_MODEL_SIZE(modelProportion) MODEL_SIZE(modelProportion) * MODEL_CHANNELS                                                                                     // Amount of bytes in model input \
                                                                                                          // Size of a single detection (4 coordinates, 1 object prob, number classes x class prob)
#define LARGE_DETECTION_COUNT(modelProportion) MODEL_SIZE(modelProportion) / (8 * 8) * 3                                                                                      // Number of detections in the largest output
#define MEDIUM_DETECTION_COUNT(modelProportion) MODEL_SIZE(modelProportion) / (16 * 16) * 3                                                                                   // Number of detections in the medium output
#define SMALL_DETECTION_COUNT(modelProportion) MODEL_SIZE(modelProportion) / (32 * 32) * 3                                                                                    // Number of detections in the smallest output
#define EXPECTED_DETECTION_COUNT(modelProportion) (LARGE_DETECTION_COUNT(modelProportion) + MEDIUM_DETECTION_COUNT(modelProportion) + SMALL_DETECTION_COUNT(modelProportion)) // Total numbers of detections

#define SINGLE_DETECTION_SIZE(classCount) (classCount + 5)
#define EXPECTED_DETECTION_SIZE(classCount, modelProportion) (SINGLE_DETECTION_SIZE(classCount) * EXPECTED_DETECTION_COUNT(modelProportion)) // Total number of data elements (floats)

#define INFERENCE_THREAD_COUNT 4
#define OBJ_PROB_THRESHOLD 0.25
#define IOU_THRESHOLD 0.45

#define GOTO_IF(assertion, label) \
    if (assertion)                \
        goto label;

G_DEFINE_TYPE(GstInferenceUtil, gst_inference_util, G_TYPE_OBJECT);

typedef struct _Coordinate Coordinate;
struct _Coordinate
{
    gfloat x, y;
};

typedef struct _WorkPackage WorkPackage;
struct _WorkPackage
{
    gint rowIndex;
    gint rowWidth;
    guint8 *in;
    gfloat *out;
};

static const gfloat scales[3] = {1.2f, 1.1f, 1.05f}; // large -> large
static const gint8 strides[3] = {8, 16, 32};         // large -> small
// large -> small, 3 each
static const Coordinate anchorCollection[3][3] = {
    {{.x = 12, .y = 16}, {.x = 19, .y = 36}, {.x = 40, .y = 28}},
    {{.x = 36, .y = 75}, {.x = 76, .y = 55}, {.x = 72, .y = 146}},
    {{.x = 142, .y = 110}, {.x = 192, .y = 243}, {.x = 459, .y = 401}},
};

// -----------------------------------------------------------------------------------------------------
// ---------------------------------------- Private Helpers --------------------------------------------
// -----------------------------------------------------------------------------------------------------

// Compute the sigmoid function
static gfloat sigmoid(gfloat x)
{
    return 1.0f / (1 + exp(-x));
}

// Get the maximum value and index from an array of floats
static void array_max(const gfloat *array, gint size, gint *maxIdx, gfloat *maxVal)
{
    *maxVal = -1;
    *maxIdx = -1;

    for (gint i = 0; i < size; i++)
    {
        if (array[i] > *maxVal)
        {
            *maxVal = array[i];
            *maxIdx = i;
        };
    }
}

// Comparator for grouping Detection structs by label
static int compare_detections(const GstDetection **a, const GstDetection **b)
{
    return strcmp((*a)->label, (*b)->label);
}

// Appends all detections from the input array to the output array
static void append_detections(GPtrArray *in, GPtrArray *out)
{
    g_ptr_array_extend(out, in, (GCopyFunc)g_object_ref, NULL);
}

// Computes the IoU (Intersection over Union) score between bounding boxes of two detections
static gfloat iou(GstDetection *a, GstDetection *b)
{
    gfloat intMinX, intMinY, intMaxX, intMaxY;
    // To get the top left corner of the intersection, use the largest left corner coordinate and largest top corner coordinate
    intMinX = fmax(a->bbox.x, b->bbox.x);
    intMinY = fmax(a->bbox.y, b->bbox.y);
    // To get the bottom right corner of the intersection, use the smallest right corner coordinate and smallest bottom corner coordinate
    intMaxX = fmin(a->bbox.x + a->bbox.width, b->bbox.x + b->bbox.width);
    intMaxY = fmin(a->bbox.y + a->bbox.height, b->bbox.y + b->bbox.height);

    gfloat areaA, areaB, interArea, unionArea;
    areaA = a->bbox.width * a->bbox.height;
    areaB = b->bbox.width * b->bbox.height;
    if (intMaxX - intMinX <= 0 || intMaxY - intMinY <= 0)
        interArea = 0;
    else
        interArea = (intMaxX - intMinX) * (intMaxY - intMinY);
    unionArea = areaA + areaB - interArea;

    return interArea / unionArea;
}

// Performs non maximum suppression on all detections of a single class
// Iteratively gets the best detection and removes all detections with a high overlap (IoU)
static void non_max_suppression(GPtrArray *detections, GPtrArray *out)
{
    while (detections->len > 0)
    {
        // Get the highest scoring detection of that class and store it
        gint bestDetectionIdx = -1;
        gfloat maxConfidence = -1;
        for (gint i = 0; i < detections->len; i++)
        {
            GstDetection *element = g_ptr_array_index(detections, i);
            if (element->confidence > maxConfidence)
            {
                maxConfidence = element->confidence;
                bestDetectionIdx = i;
            }
        }

        // Add best detection to the output and remove it from input
        g_ptr_array_add(out, g_ptr_array_remove_index(detections, bestDetectionIdx));

        // Remove all detections that have a high IoU with the currently best detection
        GstDetection *bestDetection = g_ptr_array_index(out, out->len - 1);
        for (gint i = 0; i < detections->len; i++)
        {
            GstDetection *element = g_ptr_array_index(detections, i);

            if (iou(bestDetection, element) > IOU_THRESHOLD)
            {
                g_ptr_array_remove_index(detections, i);
                i--; // Remaining elements shift back by one, so we need to check the same index again
            }
        }

        // Repeat with the remaining detections
    }
}

// Convert raw output of NN
// Scales x, y, width & height to the input dimensions of the NN
// Weighs the class probabilities by the object probability
static void parse_detections(gfloat *detections, gint detectionCount, gint classCount, gfloat *out)
{
    for (gint i = 0; i < detectionCount; i++)
    {
        gint offset = i * SINGLE_DETECTION_SIZE(classCount);

        gfloat centerX, centerY, x, y, width, height;
        gfloat objProb;
        // As MSVC does not support variable length arrays, we need to allocate this on the heap...
        gfloat *rawClassProbs = g_malloc(sizeof(gfloat) * classCount);
        gfloat *classProbs = g_malloc(sizeof(gfloat) * classCount);

        centerX = detections[offset + 0];
        centerY = detections[offset + 1];
        width = detections[offset + 2];
        height = detections[offset + 3];
        x = centerX - width / 2.0f;
        y = centerY - height / 2.0f;

        // Process probabilities
        memcpy(rawClassProbs, &detections[offset + 5], classCount * sizeof(gfloat));
        objProb = detections[offset + 4];
        for (gint i = 0; i < classCount; i++)
        {
            classProbs[i] = objProb * rawClassProbs[i];
        }

        // Write to output
        out[offset + 0] = x;
        out[offset + 1] = y;
        out[offset + 2] = width;
        out[offset + 3] = height;
        out[offset + 4] = objProb;
        memcpy(&out[offset + 5], classProbs, classCount * sizeof(gfloat));

        g_free(classProbs);
        g_free(rawClassProbs);
    }
}

// Convert single image line
static void convert_row(WorkPackage *package)
{
    for (gint col = 0; col < package->rowWidth; col++) // Col
    {
        const gint rowOffset = package->rowIndex * package->rowWidth;
        const gint inPixelOffset = (rowOffset + col) * INPUT_CHANNELS; // Every input component has 4 bytes (BGRA)

        for (gint chan = 0; chan < MODEL_CHANNELS; chan++)
        {
            // We store each color layer continuously, we need to invert channel order
            const gint outPixelOffset = (rowOffset + col) + (MODEL_CHANNELS - chan - 1) * MODEL_SIZE(package->rowWidth);
            package->out[outPixelOffset] = package->in[inPixelOffset + chan] / 255.0f; // Convert to float
        }
    }

    g_free(package);
}

// Convert a 4 channel uint8 BGRA frame to a 3 channel float RGB frame in 3xMODEL_SIZExMODEL_SIZE shape (every color plane stored after one another)
static void image_to_float(gint width, gint height, guint8 *in, gfloat *out)
{
    GThreadPool *pool = g_thread_pool_new((GFunc)convert_row, NULL, 4, FALSE, NULL);

    for (gint row = 0; row < height; row++) // Row
    {
        WorkPackage *package = g_malloc(sizeof(WorkPackage));
        package->rowIndex = row;
        package->in = in;
        package->out = out;
        package->rowWidth = width;

        g_thread_pool_push(pool, package, NULL);
    }

    g_thread_pool_free(pool, FALSE, TRUE);
}

// -----------------------------------------------------------------------------------------------------
// ----------------------------------------- Public Helpers --------------------------------------------
// -----------------------------------------------------------------------------------------------------

void inference_couple(GstInferenceData *source, GstInferenceData *target)
{
    // Add last detections to this data
    g_ptr_array_unref(target->detections);
    target->detections = g_ptr_array_ref(source->detections);
}

gboolean inference_apply(GstObjDetection *objDet, GstInferenceData *data)
{
    // Check if bypass buffer is writeable
    g_return_val_if_fail(gst_buffer_is_writable(data->bypassBuffer), FALSE);

    // Prepare meta on bypass buffer
    GstDetectionMeta *meta;
    meta = GST_DETECTION_META_GET(data->bypassBuffer);
    if (meta == NULL)
    {
        // Add new detection meta
        GST_DEBUG_OBJECT(objDet, "No object detection meta on buffer. Adding new meta.");
        meta = GST_DETECTION_META_ADD(data->bypassBuffer);
    }

    // Add detections to buffer
    append_detections(data->detections, meta->detections);

    return TRUE;
}

// -----------------------------------------------------------------------------------------------------
// ----------------------------------------- Object Methods --------------------------------------------
// -----------------------------------------------------------------------------------------------------

// Process frame before inference
static void gst_inference_util_preprocess(GstInferenceUtil *self, guint8 *rawData, gfloat *processedData)
{
    image_to_float(self->modelProportion, self->modelProportion, rawData, processedData);
}

// Perform actual inference and extracts the output
static void gst_inference_util_infer(GstInferenceUtil *self, OrtStatus **status, gfloat *processedData, gfloat *outDetections)
{
    // Prepare memory for input tensor
    OrtMemoryInfo *memoryInfo;
    *status = self->ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memoryInfo);
    GOTO_IF(*status != NULL, out);

    // Set input tensor from preprocessed data
    const gint64 shape[4] = {1, 3, self->modelProportion, self->modelProportion};
    OrtValue *inputTensor;
    *status = self->ort->CreateTensorWithDataAsOrtValue(memoryInfo, processedData, EXPECTED_MODEL_SIZE(self->modelProportion) * sizeof(gfloat), shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &inputTensor);
    GOTO_IF(*status != NULL, out);

    // Check correct creation
    int isTensor;
    *status = self->ort->IsTensor(inputTensor, &isTensor);
    GOTO_IF(*status != NULL || !isTensor, out);

    // Define the output tensor
    OrtValue *outputTensor = NULL;

    // Define the names of the input and output neurons
    const char *inputNodeName = "images";
    const char *outputNodeName = "output"; // Combined (10647 x (5 + CLASS_COUNT)) = (52*52*3 + 26*26*3 + 13*13*3) x (5 + CLASS_COUNT)

    // Run inference
    g_mutex_lock(&self->mtxBlocked);
    while (self->sessionBlocked)
        g_cond_wait(&self->condBlocked, &self->mtxBlocked);
    g_mutex_unlock(&self->mtxBlocked);

    g_mutex_lock(&self->mtxRefCount);
    self->sessionRefCount++; // No need to signal here, as increasing the refcount won't lead to a success case anyways
    g_mutex_unlock(&self->mtxRefCount);

    *status = self->ort->Run(self->session, NULL, &inputNodeName, (const OrtValue *const *)&inputTensor, 1, &outputNodeName, 1, &outputTensor);

    g_mutex_lock(&self->mtxRefCount);
    self->sessionRefCount--;
    g_cond_signal(&self->condRefCount); // Signal potentially waiting reinit thread
    g_mutex_unlock(&self->mtxRefCount);

    GOTO_IF(*status != NULL, out);

    // Check correct inference and get pointer to detections
    gfloat *detections;
    *status = self->ort->IsTensor(outputTensor, &isTensor);
    GOTO_IF(*status != NULL || !isTensor, out);

    *status = self->ort->GetTensorMutableData(outputTensor, (void **)&detections);
    GOTO_IF(*status != NULL, out);

    // Copy prediction data
    parse_detections(detections, EXPECTED_DETECTION_COUNT(self->modelProportion), self->classCount, outDetections);

out:
    self->ort->ReleaseValue(outputTensor);
    self->ort->ReleaseMemoryInfo(memoryInfo);
}

// Performs all necessary output steps to get detections from the raw data
// Scales the coordinates to the original input size
// Filters detections with low object probability
// Performs non maximum suppression on the remaining detections
static void gst_inference_util_postprocess(GstInferenceUtil *self, gfloat *rawDetections, GPtrArray *out, const char *labelPrefix, GPtrArray *labels, gint targetWidth, gint targetHeight)
{
    const gfloat ratioX = (gfloat)targetWidth / self->modelProportion;
    const gfloat ratioY = (gfloat)targetHeight / self->modelProportion;

    // Get the highest scoring class and its score for each detection
    GPtrArray *detections = g_ptr_array_new();
    for (gint i = 0; i < EXPECTED_DETECTION_COUNT(self->modelProportion); i++)
    {
        gint offset = i * SINGLE_DETECTION_SIZE(self->classCount);

        gint x, y, width, height;
        gfloat objProp;
        gfloat *classProbs = g_malloc(sizeof(gfloat) * self->classCount);

        objProp = rawDetections[offset + 4];
        if (objProp < OBJ_PROB_THRESHOLD)
            continue;

        x = (int)roundf(rawDetections[offset + 0] * ratioX);
        y = (int)roundf(rawDetections[offset + 1] * ratioY);
        width = (int)roundf(rawDetections[offset + 2] * ratioX);
        height = (int)roundf(rawDetections[offset + 3] * ratioY);
        memcpy(classProbs, &rawDetections[offset + 5], self->classCount * sizeof(gfloat));

        // Clip bounding boxes
        x = MAX(MIN(x, targetWidth), 0);
        y = MAX(MIN(y, targetHeight), 0);
        width = MAX(MIN(width, targetWidth - x), 0);
        height = MAX(MIN(height, targetHeight - y), 0);

        gint maxConfLabel;
        gfloat maxConfScore;
        array_max(classProbs, self->classCount, &maxConfLabel, &maxConfScore);

        GString *label = g_string_new(labelPrefix);
        if (maxConfLabel < labels->len)
            g_string_append_printf(label, ":%s", g_ptr_array_index(labels, maxConfLabel));
        else
            g_string_append_printf(label, ":%i", maxConfLabel);

        BoundingBox bbox = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
        };
        GstDetection *detection = gst_detection_new(label->str, maxConfScore, bbox);

        g_ptr_array_add(detections, detection);

        g_string_free(label, TRUE);
        g_free(classProbs);
    }

    // Early return for empty detections
    if (detections->len == 0)
    {
        goto out;
    }

    // Sort the detections by class to get same classes next one another
    g_ptr_array_sort(detections, (GCompareFunc)compare_detections);

    // Do non maximum suppression per class
    GPtrArray *group = g_ptr_array_new();
    const char *currentLabel = ((GstDetection *)g_ptr_array_index(detections, 0))->label;
    for (gint i = 0; i < detections->len; i++)
    {
        GstDetection *element = g_ptr_array_index(detections, i);

        if (!element->label || !currentLabel || g_str_equal(element->label, currentLabel))
        {
            // Element still belongs to current class, so transfer it to group
            g_ptr_array_add(group, g_ptr_array_remove_index(detections, i));
            continue;
        }

        // New label at this index, new group starts, so the last group is complete
        non_max_suppression(group, out);
        // NMS will remove all elements from the group so we can directly reuse it

        i = 0;
        currentLabel = element->label;
    }

    // Handle last group (i.e. the remaining detections)
    non_max_suppression(group, out);
    g_ptr_array_free(group, FALSE);

out:
    g_ptr_array_free(detections, FALSE);
}

// Public inference function. Handles all required inference steps
gboolean gst_inference_util_run_inference(GstInferenceUtil *self, GstObjDetection *objDet, GstInferenceData *data)
{
    GST_DEBUG_OBJECT(objDet, "Starting inference");

    gboolean ret = TRUE;
    OrtStatus *status = NULL;
    gfloat *processedData = NULL, *rawDetections = NULL;

    // Check if initialized & ORT API is available
    if (!self->initialized || self->ort == NULL)
        return FALSE;
    // Check if bypass buffer is writeable
    g_return_val_if_fail(gst_buffer_is_writable(data->bypassBuffer), FALSE);

    // Check if image from buffer has right size
    GstMapInfo info;
    gst_buffer_map(data->modelBuffer, &info, GST_MAP_READ);
    if (info.size != EXPECTED_INPUT_SIZE(self->modelProportion))
    {
        GST_ERROR_OBJECT(objDet, "ModelBuffer has wrong size for inference. Got %zu, expected %i", info.size, EXPECTED_INPUT_SIZE(self->modelProportion));
        ret = FALSE;
        goto out;
    }

    // Preprocess data
    processedData = g_malloc(EXPECTED_MODEL_SIZE(self->modelProportion) * sizeof(gfloat));
    gst_inference_util_preprocess(self, info.data, processedData);
    GST_DEBUG_OBJECT(objDet, "Finished preprocessing");

    // Run inference
    rawDetections = g_malloc(EXPECTED_DETECTION_SIZE(self->classCount, self->modelProportion) * sizeof(gfloat));
    gst_inference_util_infer(self, &status, processedData, rawDetections);
    GST_DEBUG_OBJECT(objDet, "Finished infering");

    // Get information on bypass buffer
    GstVideoMeta *videoMeta = (GstVideoMeta *)gst_buffer_get_meta(data->bypassBuffer, GST_VIDEO_META_API_TYPE);
    if (videoMeta == NULL)
    {
        GST_ERROR_OBJECT(objDet, "No video metadata available");
        ret = FALSE;
        goto out;
    }

    // Postprocess the output (automatically adds detections to the metadata)
    gst_inference_util_postprocess(self, rawDetections, data->detections, objDet->prefix, objDet->labels, videoMeta->width, videoMeta->height);
    GST_DEBUG_OBJECT(objDet, "Finished postprocess");

out:
    if (status != NULL)
    {
        const char *errMessage = self->ort->GetErrorMessage(status);
        GST_ERROR_OBJECT(objDet, "%s", errMessage);

        ret = FALSE;
        self->ort->ReleaseStatus(status);
    }

    gst_buffer_unmap(data->modelBuffer, &info);
    if (processedData)
        g_free(processedData);
    if (rawDetections)
        g_free(rawDetections);

    GST_DEBUG_OBJECT(objDet, "Finished inference");
    return ret;
}

// Create an onnxruntime session
static OrtStatusPtr gst_inference_util_create_session(GstInferenceUtil *self, GstObjDetection *objDet)
{
    OrtStatusPtr status;
#ifdef WIN32
    int wcharCharacterCount = MultiByteToWideChar(CP_UTF8, 0, objDet->modelPath, -1, NULL, 0);
    wchar_t *wideModelPath = g_malloc(wcharCharacterCount * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, objDet->modelPath, -1, wideModelPath, wcharCharacterCount);
    status = self->ort->CreateSession(self->environment, wideModelPath, self->options, &self->session);
    g_free((void *)wideModelPath);
#else
    status = self->ort->CreateSession(self->environment, objDet->modelPath, self->options, &self->session);
#endif

    // Early return on error
    GOTO_IF(status != NULL, out);

    OrtTypeInfo *typeInfo = NULL;
    const OrtTensorTypeAndShapeInfo *tensorInfo = NULL;
    size_t dimCount;
    gint64 *dimensions;

    // Get model width/height
    // Get input tensor and retrieve its info
    status = self->ort->SessionGetInputTypeInfo(self->session, 0, &typeInfo);
    GOTO_IF(status != NULL, out);

    status = self->ort->CastTypeInfoToTensorInfo(typeInfo, &tensorInfo); // No need to free tensorInfo
    GOTO_IF(status != NULL, out);

    status = self->ort->GetDimensionsCount(tensorInfo, &dimCount);
    GOTO_IF(status != NULL, out);

    dimensions = g_malloc(dimCount * sizeof(gint64));
    status = self->ort->GetDimensions(tensorInfo, dimensions, dimCount);
    GOTO_IF(status != NULL, out);

    // Get last element dimension (i.e. model width/height)
    self->modelProportion = (gint)dimensions[dimCount - 1];
    gst_obj_detection_reconfigure_model_sink(objDet, self->modelProportion);

    // Free memory
    g_free((void *)dimensions);
    self->ort->ReleaseTypeInfo(typeInfo);

    // Get class count
    // Get output tensor and retrieve its info
    status = self->ort->SessionGetOutputTypeInfo(self->session, 0, &typeInfo);
    GOTO_IF(status != NULL, out);

    status = self->ort->CastTypeInfoToTensorInfo(typeInfo, &tensorInfo); // No need to free tensorInfo
    GOTO_IF(status != NULL, out);

    status = self->ort->GetDimensionsCount(tensorInfo, &dimCount);
    GOTO_IF(status != NULL, out);

    dimensions = g_malloc(dimCount * sizeof(gint64));
    status = self->ort->GetDimensions(tensorInfo, dimensions, dimCount);
    GOTO_IF(status != NULL, out);

    // Get last element dimension (i.e. detection size)
    gint fullDetectionSize = (gint)dimensions[dimCount - 1];
    self->classCount = fullDetectionSize - 5;

    // Free memory
    g_free((void *)dimensions);
    self->ort->ReleaseTypeInfo(typeInfo);

    // Get label names from meta
    OrtModelMetadata *modelMeta = NULL;
    status = self->ort->SessionGetModelMetadata(self->session, &modelMeta);
    GOTO_IF(status != NULL, out);

    // Retrieve custom metadata
    OrtAllocator *allocator = NULL;
    status = self->ort->GetAllocatorWithDefaultOptions(&allocator); // No need to free allocator
    GOTO_IF(status != NULL, out);

    // Clear existing labels
    while (objDet->labels->len)
        g_ptr_array_remove_index(objDet->labels, 0);

    char *labelString;
    status = self->ort->ModelMetadataLookupCustomMetadataMap(modelMeta, allocator, "labels", &labelString);
    GOTO_IF(status != NULL, out);

    if (labelString != NULL)
    {
        // Split at comma and fill
        char **labels = g_strsplit(labelString, ",", -1);
        for (gint i = 0; labels[i]; i++)
        {
            char *label = labels[i];
            g_ptr_array_add(objDet->labels, g_strdup(label));
        }
        g_strfreev(labels);
    }

    // Free memory
    self->ort->ReleaseModelMetadata(modelMeta);
    allocator->Free(allocator, labelString);

out:
    return status;
}

// Actual inference initialization
void gst_inference_util_initialize(GstInferenceUtil *self, GstObjDetection *objDet)
{
    if (self->initialized)
    {
        GST_WARNING("Already initialized inference");
        return;
    }

    if (self->ort == NULL)
    {
        GST_WARNING("Onnxruntime api undefined, aborting initialization");
        return;
    }

    if (objDet->modelPath == NULL)
    {
        GST_WARNING("Model path undefined, aborting initialization");
        return;
    }

    OrtStatus *status = NULL;
    gboolean ret = TRUE;

    // Setup session
    status = self->ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "objdetection", &self->environment);
    GOTO_IF(status != NULL, out);
    status = self->ort->CreateSessionOptions(&self->options);
    GOTO_IF(status != NULL, out);
    status = self->ort->SetIntraOpNumThreads(self->options, INFERENCE_THREAD_COUNT);
    GOTO_IF(status != NULL, out);
    status = self->ort->SetSessionGraphOptimizationLevel(self->options, ORT_ENABLE_ALL);
    GOTO_IF(status != NULL, out);

    // Create session
    status = gst_inference_util_create_session(self, objDet);
    GOTO_IF(status != NULL, out);

out:
    if (status != NULL)
    {
        const char *errMessage = self->ort->GetErrorMessage(status);
        GST_ERROR("%s", errMessage);

        ret = FALSE;
        self->ort->ReleaseStatus(status);
    }

    self->initialized = ret;
}

// Updates the used onnxmodel
void gst_inference_util_reinitialize(GstInferenceUtil *self, GstObjDetection *objDet)
{
    if (!self->initialized)
    {
        GST_WARNING("Inference has not been initialized yet, running initialize instead");
        gst_inference_util_initialize(self, objDet);
        return;
    }

    OrtStatus *status = NULL;
    gboolean ret = TRUE;

    // Block inference threads
    g_mutex_lock(&self->mtxBlocked);
    self->sessionBlocked = TRUE;
    g_mutex_unlock(&self->mtxBlocked);

    // Wait until inference threads are all blocked
    g_mutex_lock(&self->mtxRefCount);
    while (self->sessionRefCount > 0)
        g_cond_wait(&self->condRefCount, &self->mtxRefCount);
    g_mutex_unlock(&self->mtxRefCount);

    // Release old session
    self->ort->ReleaseSession(self->session);

    // Create new session
    status = gst_inference_util_create_session(self, objDet);
    GOTO_IF(status != NULL, out);

    g_mutex_lock(&self->mtxBlocked);
    self->sessionBlocked = FALSE;
    g_cond_signal(&self->condBlocked); // Signal potentially waiting infer threads
    g_mutex_unlock(&self->mtxBlocked);

out:
    if (status != NULL)
    {
        const char *errMessage = self->ort->GetErrorMessage(status);
        GST_ERROR("%s", errMessage);

        ret = FALSE;
        self->ort->ReleaseStatus(status);
    }

    self->initialized = ret;
}

void gst_inference_util_finalize(GstInferenceUtil *self)
{
    if (!self->initialized)
    {
        GST_WARNING("Nothing to finalize, inference not initialized");
        return;
    }

    g_mutex_lock(&self->mtxRefCount);
    while (self->sessionRefCount > 0)
        g_cond_wait(&self->condRefCount, &self->mtxRefCount);
    g_mutex_unlock(&self->mtxRefCount);

    // Dispose of the model and interpreter objects
    // Close ONNX session
    self->ort->ReleaseSession(self->session);
    self->ort->ReleaseSessionOptions(self->options);
    self->ort->ReleaseEnv(self->environment);

    // Clean session variables
    self->classCount = -1;

    // Reset thread management
    self->sessionRefCount = 0;
    self->sessionBlocked = FALSE;

    self->initialized = FALSE;
}

// Object instantiation
static void gst_inference_util_init(GstInferenceUtil *self)
{
    // Set default values
    self->initialized = FALSE;
    self->ort = NULL;

    g_mutex_init(&self->mtxBlocked);
    g_mutex_init(&self->mtxRefCount);

    g_cond_init(&self->condBlocked);
    g_cond_init(&self->condRefCount);

    self->sessionRefCount = 0;
    self->sessionBlocked = FALSE;

// Initialize onnxruntime API
#ifdef WIN32
    // On Windows, use run-time dynamic linking due to the conflicting onnxruntime version in System32
    if (self->module != NULL)
        goto skip_module;

    // Get handle to current dll by looking up a local symbol
    HMODULE hm = NULL;
    if (!GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&self->ort,
            &hm))
    {
        g_warning(stderr, "Unable to get module handle of current dll");
        goto module_error;
    }

    // Get path of current dll
    char modulePath[MAX_PATH];
    GetModuleFileNameA(hm, modulePath, sizeof(modulePath));
    char *moduleDir = g_path_get_dirname(modulePath);
    char *onnxruntimePath = g_build_filename(moduleDir, "onnxruntime.dll", NULL);
    g_free((void *)moduleDir);

    self->module = g_module_open(onnxruntimePath, G_MODULE_BIND_LOCAL);
    if (self->module == NULL)
    {
        g_warning("Unable to open onnxruntime at %s, err: %s", onnxruntimePath, g_module_error());
        g_free((void *)onnxruntimePath);
        goto module_error;
    }
    g_free((void *)onnxruntimePath);

skip_module:
    // Get pointer to get base api function
    OrtApiBase *(*OrtGetApiBaseLocal)(void);
    if (!g_module_symbol(self->module, "OrtGetApiBase", (void **)&OrtGetApiBaseLocal))
    {
        g_warning("Unable to find \"OrtGetApiBase\" symbol, err: %s", g_module_error());
        g_module_close(self->module);
        goto module_error;
    }

    // Create ORT API fromlocal library
    self->ort = OrtGetApiBaseLocal()->GetApi(ORT_API_VERSION);
module_error:;
#else
    // On Mac and Linux, use load-time dynamic linking
    self->ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
#endif
}

// Object finalization
static void gst_inference_util_object_finalize(GObject *object)
{
    GstInferenceUtil *self = GST_INFERENCE_UTIL(object);

    // Default session finalization
    gst_inference_util_finalize(self);

    // Reset thread management
    g_mutex_clear(&self->mtxBlocked);
    g_mutex_clear(&self->mtxRefCount);
    g_cond_clear(&self->condBlocked);
    g_cond_clear(&self->condRefCount);

#ifdef WIN32
    // Onnxruntime management
    g_module_close(self->module);
#endif

    G_OBJECT_CLASS(gst_inference_util_parent_class)->finalize(object);
}

static void gst_inference_util_class_init(GstInferenceUtilClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gst_inference_util_object_finalize;
}

GstInferenceUtil *gst_inference_util_new()
{
    return g_object_new(GST_TYPE_INFERENCE_UTIL, NULL);
}
