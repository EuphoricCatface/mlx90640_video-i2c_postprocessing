#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/controller/gsttriggercontrolsource.h>
#include <gst/controller/gstinterpolationcontrolsource.h>
#include <gst/controller/gstdirectcontrolbinding.h>

#include <cstdio>

#include "push_data.hpp"

#define CHUNK_SIZE (32 * 24 * 2)   /* Amount of bytes we are sending in each buffer */

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
    GstElement *pipeline, *app_source, *video_scale, *caps_filter;
    GstElement *gl_upload, *gl_colorconvert, *gl_effects_heat;
    GstElement *gl_overlay_center, *gl_overlay_hot, *gl_overlay_cold;
    GstElement *app_src_txt, *text_overlay, *gl_imagesink;

    GstBuffer *buffer;
    GstMapInfo map;

    GstControlSource * hot_x;
    GstControlSource * hot_y;
    GstControlSource * cold_x;
    GstControlSource * cold_y;
    int scale_ratio;

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

bool gst_arm_buffer(const mlx90640::notable_pxls_t * const pix_list) {
    GstFlowReturn ret;
    GstFlowReturn ret_txt;
    char overlay_str[64];

    if (_data == NULL || _data->buffer == NULL)
        return false;


    /*
    GstClockTime overlay_time =
        gst_clock_get_time(
            gst_pipeline_get_clock(
        	    GST_PIPELINE_CAST(_data->pipeline)
            ) )
        - GST_SECOND / 2;
    */
    // getting time: from gstappsrc source code
    // https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/blob/1.14.4/gst-libs/gst/app/gstappsrc.c
    GstClock *clock = gst_element_get_clock (GST_ELEMENT_CAST (_data->app_source));
    GstClockTime base_time =
        gst_element_get_base_time (GST_ELEMENT_CAST (_data->app_source));
    GstClockTime now = gst_clock_get_time (clock);
    if (now > base_time)
        now = now - base_time - 0.1 * GST_SECOND;
    else
        now = 0;
    gst_object_unref (clock);
    //g_print("=========old_now: %llu, now: %llu, base_time: %llu\n", old_now, now, base_time);

    /* Set the buffer's timestamp and duration - NOT */
    // http://gstreamer-devel.966125.n4.nabble.com/How-do-you-construct-the-timestamps-duration-for-video-audio-appsrc-when-captured-by-DeckLink-tp4675678p4675748.html
    // You set do-timestamp to true.
    GST_BUFFER_DTS(_data->buffer) = now;
    GST_BUFFER_PTS(_data->buffer) = now;

    /* TODO: Ideally, we can start & stop the camera,
     * but let's just discard *ALL* the data for the time being */
    if (!_data->feed_running)
        return TRUE; // eeeeeehhhh... we're *not* experiencing a problem, right?

    gst_buffer_unmap (_data->buffer, &(_data->map));

    /* Push the buffer into the app_src_txt */
    GstBuffer * txtbuf;

    snprintf(overlay_str, 64, "MAX: %.2lf\nMIN: %.2lf\nMID: %.2lf",
        (*pix_list)[mlx90640::MAX_T].T,
        (*pix_list)[mlx90640::MIN_T].T,
        (*pix_list)[mlx90640::SCENE_CENTER].T);

    txtbuf = gst_buffer_new_wrapped_bytes (
        g_string_free_to_bytes (
            g_string_new(overlay_str)
        )
    );
    GST_BUFFER_DTS(txtbuf) = now;
    GST_BUFFER_PTS(txtbuf) = now;

    /* Set the indicator coordinate */
    g_object_set (_data->gl_overlay_hot,
                "location", "18231_rgb.png",
                "offset_x", _data->scale_ratio * (*pix_list)[mlx90640::MAX_T].x,
                "offset_y", _data->scale_ratio * (*pix_list)[mlx90640::MAX_T].y,
                NULL);
    g_object_set (_data->gl_overlay_cold,
                "location", "18231_blue.png",
                "offset_x", _data->scale_ratio * (*pix_list)[mlx90640::MIN_T].x,
                "offset_y", _data->scale_ratio * (*pix_list)[mlx90640::MIN_T].y,
                NULL);
/*
    GstClockTime slight_future = now + GST_SECOND / 20;
    GstClockTime slight_past = now - GST_SECOND / 20;
    GstTimedValueControlSource *tv_hx = (GstTimedValueControlSource *) _data->hot_x;
    GstTimedValueControlSource *tv_hy = (GstTimedValueControlSource *) _data->hot_y;
    GstTimedValueControlSource *tv_cx = (GstTimedValueControlSource *) _data->cold_x;
    GstTimedValueControlSource *tv_cy = (GstTimedValueControlSource *) _data->cold_y;

    gst_timed_value_control_source_set(tv_hx, slight_past, 0);
    gst_timed_value_control_source_set(tv_hy, slight_past, 0);
    gst_timed_value_control_source_set(tv_cx, slight_past, 0);
    gst_timed_value_control_source_set(tv_cy, slight_past, 0);

    gst_timed_value_control_source_set(tv_hx, now, _data->scale_ratio * (*pix_list)[mlx90640::MAX_T].x);
    gst_timed_value_control_source_set(tv_hy, now, _data->scale_ratio * (*pix_list)[mlx90640::MAX_T].y);
    gst_timed_value_control_source_set(tv_cx, now, _data->scale_ratio * (*pix_list)[mlx90640::MIN_T].x);
    gst_timed_value_control_source_set(tv_cy, now, _data->scale_ratio * (*pix_list)[mlx90640::MIN_T].y);

    //gst_timed_value_control_source_set(tv_hx, now, 0);
    //gst_timed_value_control_source_set(tv_hy, now, 0);
    //gst_timed_value_control_source_set(tv_cx, now, 0);
    //gst_timed_value_control_source_set(tv_cy, now, 0);

    gst_timed_value_control_source_set(tv_hx, slight_future, _data->scale_ratio * (*pix_list)[mlx90640::MAX_T].x);
    gst_timed_value_control_source_set(tv_hy, slight_future, _data->scale_ratio * (*pix_list)[mlx90640::MAX_T].y);
    gst_timed_value_control_source_set(tv_cx, slight_future, _data->scale_ratio * (*pix_list)[mlx90640::MIN_T].x);
    gst_timed_value_control_source_set(tv_cy, slight_future, _data->scale_ratio * (*pix_list)[mlx90640::MIN_T].y);
*/
    /* Push the buffer into the appsrc */
    ret = gst_app_src_push_buffer((GstAppSrc *)(_data->app_source), _data->buffer);
    _data->buffer = NULL;
    ret_txt = gst_app_src_push_buffer((GstAppSrc *)(_data->app_src_txt), txtbuf);

    if (ret != GST_FLOW_OK || ret_txt != GST_FLOW_OK) {
        /* We got some error, stop sending data */
        return FALSE;
    }

    return TRUE;
}

/*
void gst_arm_pixeldata() {
    GstTimedValueControlSource *tv_csource = (GstTimedValueControlSource *)(_data->csource);
    gst_timed_value_control_source_set (tv_csource, 0, "test2");
}
*/

/* This signal callback triggers when appsrc needs data. */
static void start_feed (GstElement * /*source*/, guint /*size*/, CustomData *data) {
    if (data->feed_running)
        return;
    g_print ("Start feeding\n");
    data->feed_running = true;
}

/* This callback triggers when appsrc has enough data and we can stop sending. */
static void stop_feed (GstElement * /*source*/, CustomData *data) {
    if (!data->feed_running)
        return;
    g_print ("Stop feeding\n");
    data->feed_running = false;
}

/* This function is called when an error message is posted on the bus */
static void error_cb (GstBus * /*bus*/, GstMessage *msg, CustomData * /*data*/) {
    GError *err;
    gchar *debug_info;

    /* Print error details on the screen */
    gst_message_parse_error (msg, &err, &debug_info);
    g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
    g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error (&err);
    g_free (debug_info);
}

int gst_init_(int scale_type, int scale_ratio) {
    _data = new CustomData;
    CustomData &data = *_data;
    GstVideoInfo info;
    GstCaps *video_caps;
    GstCaps *text_caps;
    GstBus *bus;

    /* Initialize cumstom data structure */
    memset (&data, 0, sizeof (data));
    data.buffer = NULL;
    data.feed_running = false;
    data.scale_ratio = scale_ratio;

    /* Initialize GStreamer */
    gst_init (NULL, NULL);

    /* Create the elements */
    data.app_source = gst_element_factory_make ("appsrc", "mlx_source");
    data.video_scale = gst_element_factory_make("videoscale", "video_scale");
    data.caps_filter = gst_element_factory_make("capsfilter", "caps_filter");

    data.gl_upload = gst_element_factory_make("glupload", "gl_upload");
    data.gl_colorconvert = gst_element_factory_make("glcolorconvert", "gl_colorconvert");
    data.gl_effects_heat = gst_element_factory_make("gleffects_heat", "gl_effects_heat");

    data.gl_overlay_center = gst_element_factory_make("gloverlay", "gl_overlay_center");
    data.gl_overlay_hot = gst_element_factory_make("gloverlay", "gl_overlay_hot");
    data.gl_overlay_cold = gst_element_factory_make("gloverlay", "gl_overlay_cold");

    data.app_src_txt = gst_element_factory_make ("appsrc", "app_src_text");
    data.text_overlay = gst_element_factory_make("textoverlay", "text_overlay");
    data.gl_imagesink = gst_element_factory_make("glimagesink", "gl_imagesink");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new ("test-pipeline");

    if (!data.pipeline || !data.app_source || !data.video_scale || !data.caps_filter ||
            !data.gl_upload || !data.gl_colorconvert || !data.gl_effects_heat ||
            !data.gl_overlay_center || !data.gl_overlay_hot || !data.gl_overlay_cold ||
            !data.app_src_txt || !data.text_overlay || !data.gl_imagesink) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    /* Configure appsrc */
    /* http://gstreamer-devel.966125.n4.nabble.com/How-do-you-construct-the-timestamps-duration-for-video-audio-appsrc-when-captured-by-DeckLink-tp4675678p4675748.html */
    gst_video_info_init(&info);
    gst_video_info_set_format (&info, GST_VIDEO_FORMAT_GRAY16_LE, 32, 24);
    video_caps = gst_video_info_to_caps (&info);
    g_object_set (data.app_source,
                    "caps", video_caps,
                    "format", GST_FORMAT_TIME,
                    "stream-type", GST_APP_STREAM_TYPE_STREAM,
                    "do-timestamp", false,
                    //"min-latency", GST_SECOND / 4/* fps */,
                    "is-live", true,
                    NULL);
    g_signal_connect (data.app_source, "need-data", G_CALLBACK (start_feed), &data);
    g_signal_connect (data.app_source, "enough-data", G_CALLBACK (stop_feed), &data);

    /* Configure videoscale */
    g_object_set (data.video_scale,
            "method", scale_type,
            //"sharpen", 1.0,
            NULL);

    /* Configure capsfilter */
    GstCaps * caps = gst_caps_new_simple (
            "video/x-raw",
            "width", G_TYPE_INT, 32*scale_ratio,
            "height", G_TYPE_INT, 24*scale_ratio,
            NULL);
    g_object_set (data.caps_filter,
            "caps", caps,
            NULL);

    /* Configure gloverlay */
    // Center
    //location=./18231_rgb.png overlay-width=16 overlay-height=16 relative-x=0.5 relative-y=0.5
    g_object_set (data.gl_overlay_center,
            "location", "18231_gray.png",
            "overlay-width", scale_ratio,
            "overlay-height", scale_ratio,
            "relative-x", 0.5,
            "relative-y", 0.5,
            "name", "CH_center",
            NULL);

    // Hot and cold
    g_object_set (data.gl_overlay_hot,
            "location", "18231_rgb.png",
            "overlay-width", scale_ratio,
            "overlay-height", scale_ratio,
            "name", "CH_hot",
            NULL);
    g_object_set (data.gl_overlay_cold,
            "location", "18231_blue.png",
            "overlay-width", scale_ratio,
            "overlay-height", scale_ratio,
            "name", "CH_cold",
            NULL);

    /* Configure app_src_txt */
    text_caps = gst_caps_new_simple("text/x-raw",
                    "format", G_TYPE_STRING, "utf8",
                    NULL);
    g_object_set (data.app_src_txt,
                    "caps", text_caps,
                    "format", GST_FORMAT_TIME,
                    "stream-type", GST_APP_STREAM_TYPE_STREAM,
                    "do-timestamp", false,
                    //"min-latency", GST_SECOND / 4/* fps */,
                    "is-live", false,
                    NULL);

    /* Configure textoverlay */
    g_object_set (data.text_overlay,
            "text", "test1",
            "font-desc", "Sans, 20",
            "valignment", 2,
            "halignment", 2,
            NULL);

    /* Dynamic parameters */
    //data.hot_x = gst_trigger_control_source_new();
    data.hot_x = gst_interpolation_control_source_new();
    gst_object_add_control_binding(
        GST_OBJECT_CAST(data.gl_overlay_hot),
        gst_direct_control_binding_new_absolute(
            GST_OBJECT_CAST(data.gl_overlay_hot), "offset-x", data.hot_x
        ) );
    //data.hot_y = gst_trigger_control_source_new();
    data.hot_y = gst_interpolation_control_source_new();
    gst_object_add_control_binding(
        GST_OBJECT_CAST(data.gl_overlay_hot),
        gst_direct_control_binding_new_absolute(
            GST_OBJECT_CAST(data.gl_overlay_hot), "offset-y", data.hot_y
        ) );

    data.cold_x = gst_interpolation_control_source_new();
    gst_object_add_control_binding(
        GST_OBJECT_CAST(data.gl_overlay_cold),
        gst_direct_control_binding_new_absolute(
            GST_OBJECT_CAST(data.gl_overlay_cold), "offset-x", data.cold_x
        ) );
    data.cold_y = gst_interpolation_control_source_new();
    gst_object_add_control_binding(
        GST_OBJECT_CAST(data.gl_overlay_cold),
        gst_direct_control_binding_new_absolute(
            GST_OBJECT_CAST(data.gl_overlay_hot), "offset-y", data.cold_y
        ) );


    //GstTimedValueControlSource *tv_cx = (GstTimedValueControlSource *) _data->hot_x;
    //GstTimedValueControlSource *tv_cy = (GstTimedValueControlSource *) _data->hot_y;
    //gst_timed_value_control_source_set(tv_cx, 0.0, _data->scale_ratio * (*pix_list)[mlx90640::MIN_T].x);
    //gst_timed_value_control_source_set(tv_cy, 0.0, _data->scale_ratio * (*pix_list)[mlx90640::MIN_T].y);
    //gst_timed_value_control_source_set(tv_cx, 0, 0);
    //gst_timed_value_control_source_set(tv_cy, 0, 0);
    //gst_timed_value_control_source_set(tv_cx, 5 * GST_SECOND, 30);
    //gst_timed_value_control_source_set(tv_cy, 5 * GST_SECOND, 30);

    /* Link all elements because they have "Always" pads */
    gst_bin_add_many (GST_BIN (data.pipeline),
            data.app_source, data.video_scale, data.caps_filter,
            data.gl_upload, data.gl_colorconvert, data.gl_effects_heat,
            data.gl_overlay_center, data.gl_overlay_hot, data.gl_overlay_cold,
            data.app_src_txt, data.text_overlay, data.gl_imagesink, NULL);
    if (gst_element_link_many (
            data.app_source, data.video_scale, data.caps_filter,
            data.gl_upload, data.gl_colorconvert, data.gl_effects_heat,
            data.gl_overlay_center, data.gl_overlay_hot, data.gl_overlay_cold,
            data.text_overlay, data.gl_imagesink, NULL) != TRUE ||
        gst_element_link(data.app_src_txt, data.text_overlay) != TRUE
            ) {
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
    //gst_control_point_free(_data->hot_x);
    //gst_control_point_free(_data->hot_y);
    //gst_control_point_free(_data->cold_x);
    //gst_control_point_free(_data->cold_y);

    gst_object_unref (data.pipeline);
    delete(_data);
}
