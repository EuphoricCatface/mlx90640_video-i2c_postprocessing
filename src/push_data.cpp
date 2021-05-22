#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <string.h>

#define CHUNK_SIZE 1024   /* Amount of bytes we are sending in each buffer */
#define SAMPLE_RATE 44100 /* Samples per second we are sending */

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
    // gst-launch-1.0 filesrc location=~/extended_test
    //                  ! rawvideoparse format=gray16-le width=32 height=24 framerate=4/1
    //                  ! glupload ! glcolorconvert ! glcolorscale ! gleffects_heat
    //                  ! gldownload ! video/x-raw, width=320, height=240 ! autovideosink
    GstElement *pipeline, *app_source, *video_queue, *gl_upload;
    GstElement *gl_colorconvert, *gl_colorscale, *gl_effects_heat;
    GstElement *gl_download, *video_sink;

    guint64 num_samples;   /* Number of samples generated so far (for timestamp generation) */
    gfloat a, b, c, d;     /* For waveform generation */
} CustomData;

static CustomData * _data;

/* This method is called by the idle GSource in the mainloop, to feed CHUNK_SIZE bytes into appsrc.
 * The idle handler is added to the mainloop when appsrc requests us to start sending data (need-data signal)
 * and is removed when appsrc has enough data (enough-data signal).
 */
static gboolean push_data (CustomData *data) {
    GstBuffer *buffer;
    GstFlowReturn ret;
    int i;
    GstMapInfo map;
    gint16 *raw;
    gint num_samples = CHUNK_SIZE / 2; /* Because each sample is 16 bits */
    gfloat freq;

    /* Create a new empty buffer */
    buffer = gst_buffer_new_and_alloc (CHUNK_SIZE);

    /* Set its timestamp and duration */
    GST_BUFFER_TIMESTAMP (buffer) = gst_util_uint64_scale (data->num_samples, GST_SECOND, SAMPLE_RATE);
    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (num_samples, GST_SECOND, SAMPLE_RATE);

    /* Generate some psychodelic waveforms */
    gst_buffer_map (buffer, &map, GST_MAP_WRITE);
    raw = (gint16 *)map.data;
    data->c += data->d;
    data->d -= data->c / 1000;
    freq = 1100 + 1000 * data->d;
    for (i = 0; i < num_samples; i++) {
        data->a += data->b;
        data->b -= data->a / freq;
        raw[i] = (gint16)(500 * data->a);
    }
    gst_buffer_unmap (buffer, &map);
    data->num_samples += num_samples;

    /* Push the buffer into the appsrc */
    g_signal_emit_by_name (data->app_source, "push-buffer", buffer, &ret);

    /* Free the buffer now that we are done with it */
    gst_buffer_unref (buffer);

    if (ret != GST_FLOW_OK) {
        /* We got some error, stop sending data */
        return FALSE;
    }

    return TRUE;
}

/* This signal callback triggers when appsrc needs data. Here, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void start_feed (GstElement *source, guint size, CustomData *data) {
    g_print ("Start feeding (not)\n");
}

/* This callback triggers when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void stop_feed (GstElement *source, CustomData *data) {
    g_print ("Stop feeding (not)\n");
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

int init(void) {
    CustomData &data = *_data;
    GstVideoInfo info;
    GstCaps *video_caps;
    GstBus *bus;

    /* Initialize cumstom data structure */
    memset (&data, 0, sizeof (data));
    data.b = 1; /* For waveform generation */
    data.d = 1;

    /* Initialize GStreamer */
    gst_init (NULL, NULL);

    /* Create the elements */
    data.app_source = gst_element_factory_make ("appsrc", "mlx_source");
    data.video_queue = gst_element_factory_make ("queue", "video_queue");
    data.gl_upload = gst_element_factory_make("glupload", "gl_upload");
    data.gl_colorconvert = gst_element_factory_make("glcolorconvert", "gl_colorconvert");
    data.gl_colorscale = gst_element_factory_make("glcolorconvert", "gl_colorscale");
    data.gl_effects_heat = gst_element_factory_make("gleffects_heat", "gl_effects_heat");
    data.gl_download = gst_element_factory_make("gldownload", "gl_download");
    data.video_sink = gst_element_factory_make ("autovideosink", "video_sink");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new ("test-pipeline");

    if (!data.pipeline || !data.app_source || !data.video_queue || !data.gl_upload ||
            !data.gl_colorconvert || !data.gl_colorscale || !data.gl_effects_heat ||
            !data.gl_download || !data.video_sink) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    /* Configure appsrc */
    gst_video_info_set_format (&info, GST_VIDEO_FORMAT_GRAY16_LE, 32, 24);
    video_caps = gst_video_info_to_caps (&info);
    g_object_set (data.app_source,
                    "caps", video_caps,
                    "format", GST_FORMAT_TIME,
                    "stream-type", GST_APP_STREAM_TYPE_STREAM,
                    NULL);
    g_signal_connect (data.app_source, "need-data", G_CALLBACK (start_feed), &data);
    g_signal_connect (data.app_source, "enough-data", G_CALLBACK (stop_feed), &data);

    /* Link all elements because they have "Always" pads */
    gst_bin_add_many (GST_BIN (data.pipeline), data.pipeline,
            data.app_source, data.video_queue, data.gl_upload,
            data.gl_colorconvert, data.gl_colorscale, data.gl_effects_heat,
            data.gl_download, data.video_sink, NULL);
    if (gst_element_link_many (data.pipeline,
            data.app_source, data.video_queue, data.gl_upload,
            data.gl_colorconvert, data.gl_colorscale, data.gl_effects_heat,
            data.gl_download, data.video_sink, NULL) != TRUE) {
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

void running(void) {
    CustomData &data = *_data;
    /* Start playing the pipeline */
    gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
}

void cleanup(void) {
    CustomData &data = *_data;
    /* Free resources */
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
    delete(_data);
}
