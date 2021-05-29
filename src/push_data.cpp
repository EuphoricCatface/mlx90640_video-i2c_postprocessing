#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>

#include <string.h>

#include "push_data.hpp"

#define CHUNK_SIZE (32 * 24 * 2)   /* Amount of bytes we are sending in each buffer */

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
    // gst-launch-1.0 filesrc location=~/extended_test
    //                  ! rawvideoparse format=gray16-le width=32 height=24 framerate=4/1
    //                  ! glupload ! glcolorconvert ! glcolorscale ! gleffects_heat
    //                  ! gldownload ! video/x-raw, width=320, height=240 ! autovideosink
    GstElement *pipeline, *app_source, *video_scale, *caps_filter, /**video_queue,*/ *gl_upload;
    GstElement *gl_colorconvert,/* *gl_colorscale,*/ *gl_effects_heat;
    //GstElement *gl_download, *video_convert, *video_sink;
    //GstElement *video_convert, *video_scale, *video_sink;
    GstElement *video_sink;

    GstBuffer *buffer;
    GstMapInfo map;

    bool feed_running;
} CustomData;

static CustomData * _data = NULL;

uint8_t * gst_get_userp(void) {
    if (_data == NULL) return NULL;

    /* Create a new empty buffer */
    if (_data->buffer == NULL) {
        _data->buffer = gst_buffer_new_and_alloc (CHUNK_SIZE);
        gst_buffer_map (_data->buffer, &(_data->map), GST_MAP_WRITE);
    }

    /* Return the data portion pointer of the buffer */
    return _data->map.data;
}

bool gst_arm_buffer() {
    GstFlowReturn ret;

    if (_data == NULL || _data->buffer == NULL)
        return false;

    /* Set the buffer's timestamp and duration */
    //FIXME
    GST_BUFFER_TIMESTAMP (_data->buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION (_data->buffer) = GST_CLOCK_TIME_NONE;

    /* TODO: Ideally, we can start & stop the camera,
     * but let's just discard *ALL* the data for the time being */
    if (!_data->feed_running)
        return TRUE; // eeeeeehhhh... we're *not* experiencing a problem, right?

    gst_buffer_unmap (_data->buffer, &(_data->map));

    /* Push the buffer into the appsrc */
    ret = gst_app_src_push_buffer((GstAppSrc *)(_data->app_source), _data->buffer);

    _data->buffer = NULL;

    if (ret != GST_FLOW_OK) {
        /* We got some error, stop sending data */
        return FALSE;
    }

    return TRUE;
}

/* This signal callback triggers when appsrc needs data. */
static void start_feed (GstElement *source, guint size, CustomData *data) {
    g_print ("Start feeding\n");
    data->feed_running = true;
}

/* This callback triggers when appsrc has enough data and we can stop sending. */
static void stop_feed (GstElement *source, CustomData *data) {
    g_print ("Stop feeding\n");
    data->feed_running = false;
}

/* This function is called when an error message is posted on the bus */
static void error_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
    GError *err;
    gchar *debug_info;

    /* Print error details on the screen */
    gst_message_parse_error (msg, &err, &debug_info);
    g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
    g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error (&err);
    g_free (debug_info);
}

int gst_init_(void) {
    _data = new CustomData;
    CustomData &data = *_data;
    GstVideoInfo info;
    GstCaps *video_caps;
    GstBus *bus;

    /* Initialize cumstom data structure */
    memset (&data, 0, sizeof (data));
    data.buffer = NULL;
    data.feed_running = false;

    /* Initialize GStreamer */
    gst_init (NULL, NULL);

    /* Create the elements */
    data.app_source = gst_element_factory_make ("appsrc", "mlx_source");
    data.video_scale = gst_element_factory_make("videoscale", "video_scale");
    data.caps_filter = gst_element_factory_make("capsfilter", "caps_filter");
    //data.video_queue = gst_element_factory_make ("queue", "video_queue");
    data.gl_upload = gst_element_factory_make("glupload", "gl_upload");
    data.gl_colorconvert = gst_element_factory_make("glcolorconvert", "gl_colorconvert");
    //data.gl_colorscale = gst_element_factory_make("glcolorscale", "gl_colorscale");
    data.gl_effects_heat = gst_element_factory_make("gleffects_heat", "gl_effects_heat");
    //data.gl_download = gst_element_factory_make("gldownload", "gl_download");
    //data.video_convert = gst_element_factory_make("videoconvert", "video_convert");
    data.video_sink = gst_element_factory_make ("glimagesink", "video_sink");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new ("test-pipeline");

    if (!data.pipeline || !data.app_source ||/**/ !data.video_scale || !data.caps_filter || /*!data.video_queue ||*/ !data.gl_upload ||
            !data.gl_colorconvert || /*!data.gl_colorscale ||*/ !data.gl_effects_heat ||
            //!data.gl_download || !data.video_convert || !data.video_sink) {
            // !data.video_convert || !data.video_scale || !data.video_sink) {
        !data.video_sink) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    /* Configure appsrc */
    gst_video_info_init(&info);
    gst_video_info_set_format (&info, GST_VIDEO_FORMAT_GRAY16_LE, 32, 24);
    video_caps = gst_video_info_to_caps (&info);
    g_object_set (data.app_source,
                    "caps", video_caps,
                    "format", GST_FORMAT_TIME,
                    "stream-type", GST_APP_STREAM_TYPE_STREAM,
                    NULL);
    g_signal_connect (data.app_source, "need-data", G_CALLBACK (start_feed), &data);
    g_signal_connect (data.app_source, "enough-data", G_CALLBACK (stop_feed), &data);

    /* Configure videoscale */
    g_object_set (data.video_scale,
            "method", 7,
            //"sharpen", 1.0,
            NULL);
    GstCaps * caps = gst_caps_new_simple (
            "video/x-raw",
            "width", G_TYPE_INT, 64,
            "height", G_TYPE_INT, 48,
            NULL);
    g_object_set (data.caps_filter,
            "caps", caps,
            NULL);
    /*
    g_object_set (data.gl_colorscale,
            "scale-x", 10,
            "scale-y", 10,
            NULL);
    */

    /* Link all elements because they have "Always" pads */
    gst_bin_add_many (GST_BIN (data.pipeline),
            data.app_source, data.video_scale, data.caps_filter, /*data.video_queue,*/ data.gl_upload,
            data.gl_colorconvert, /*data.gl_colorscale,*/ data.gl_effects_heat,
            //data.gl_download, data.video_convert, data.video_sink, NULL);
            //data.video_convert, data.video_scale, data.video_sink, NULL);
            data.video_sink, NULL);
    if (gst_element_link_many (
            data.app_source, /*data.video_queue,*/ data.video_scale, data.caps_filter, data.gl_upload,
            data.gl_colorconvert, /*data.gl_colorscale,*/ data.gl_effects_heat,
            //data.gl_download, data.video_convert, data.video_sink, NULL) != TRUE) {
            //data.video_convert, data.video_scale, data.video_sink, NULL) != TRUE) {
            data.video_sink, NULL) != TRUE) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (data.pipeline);
        return -1;
    }

    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    bus = gst_element_get_bus (data.pipeline);
    gst_bus_add_signal_watch (bus);
    g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb, &data);
    gst_object_unref (bus);

    return 0;
}

void gst_start_running(void) {
    if (_data == NULL) return;
    CustomData &data = *_data;
    /* Start playing the pipeline */
    gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
}

void gst_cleanup(void) {
    if (_data == NULL) return;
    CustomData &data = *_data;
    /* Free resources */
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
    delete(_data);
}
