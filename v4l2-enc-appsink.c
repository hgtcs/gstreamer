#include <gst/gst.h>
#include <gst/app/app.h>
#include <stdio.h>

//十亿，10^9，Nanosecond 0
#define GST_NANOSECOND 1000000000

//unit: seconds,编码视频的时长
#define DURATION_OF_ENCVIDEO 20

FILE *fp;
GstElement *pipeline;

static GstFlowReturn
save_to_local(GstMapInfo *map)
{
    int dataLength;
    guint8 *rdata;

    int size, ret;

    dataLength = map->size;
    rdata = map->data;
    g_print("%s dataLen = %d\n", __func__, dataLength);

    size = sizeof(*rdata);
    ret = fwrite(rdata, size, dataLength, fp);
    g_print("fwrite success %d count\n", ret);
}

static GstFlowReturn
on_new_sample_from_sink(GstElement *elt)
{
    GstSample *sample;
    GstBuffer *app_buffer, *buffer;
    GstFlowReturn ret;
    GstMapInfo map;
    guint8 *rdata;
    int dataLength;

    g_print("%s\n", __func__);

    // get the sample from appsink 
    sample = gst_app_sink_pull_sample(GST_APP_SINK(elt));
    buffer = gst_sample_get_buffer(sample);

    // make a copy */
    app_buffer = gst_buffer_copy_deep(buffer);

    gst_buffer_map(app_buffer, &map, GST_MAP_WRITE);
    
    // Here You save your buffer data
    save_to_local(&map);

    // we don't need the appsink sample anymore
    gst_buffer_unmap(app_buffer, &map);

    return ret;
}

gboolean
time_ctrl()
{
    gint64 position = 0;
    guint64 duration;

    duration = DURATION_OF_ENCVIDEO;

    GstFormat format = GST_FORMAT_TIME;

    gst_element_query_position(pipeline, format, &position);
    
    if (position >= duration * GST_NANOSECOND)
    {
        GstEvent *event = gst_event_new_eos();
        if (event)
        {
            if (!gst_element_send_event(pipeline, event))
            {
                g_print("send EOS event to pipeline failed.\n");
                return -1;
            }
        }
    }
}

static gboolean
bus_callback(GstBus *bus, GstMessage *message, gpointer data)
{
    GMainLoop *loop = (GMainLoop *)data;
    switch (GST_MESSAGE_TYPE(message))
    {
    case GST_MESSAGE_ERROR:
    {
        GError *err;
        gchar *debug;
        gst_message_parse_error(message, &err, &debug);
        g_print("Error: %s\n", err->message);
        g_error_free(err);
        g_free(debug);
        g_main_loop_quit(loop);
        g_main_loop_unref(loop);
        break;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline))
        {
            g_print("Pipeline state changed from %s to %s:\n", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
        }
        break;
    }
    case GST_MESSAGE_EOS:
        g_main_loop_quit(loop);
        g_main_loop_unref(loop);
        break;
    default:
        break;
    }
    return TRUE;
}

int main(int argc, char *argv[])
{
    GstElement *src, *srccapsfilter, *timeoverlay, *enc, *enc_queue, *sink;
    GMainLoop *loop;
    GstBus *bus;
    GstCaps *srcCaps;

    gint32 ret;

    // Initialize GStreamer
    gst_init(&argc, &argv);

    //每隔一秒执行一次
    g_timeout_add_seconds(1, (GSourceFunc)time_ctrl, NULL);

    loop = g_main_loop_new(NULL, FALSE);

    if ((fp = fopen("test.h264", "a+")) == NULL)
    {
        g_print("Fail to open file!\n");
        return -1;
    }

    if (argc != 1)
    {
        g_print("usage: %s <src_uri *.yuv> <sink_uri *.h264>\n", argv[0]);
        return -1;
    }

    //Create the empty pipeline
    pipeline = gst_pipeline_new("usb-264enc");

    // Build the element
    src                 = gst_element_factory_make("v4l2src", NULL);
    srccapsfilter       = gst_element_factory_make("capsfilter", NULL);
    timeoverlay         = gst_element_factory_make("timeoverlay", NULL);
    enc                 = gst_element_factory_make("x264enc", NULL);
    enc_queue           = gst_element_factory_make("queue", NULL);
    sink                = gst_element_factory_make("appsink", NULL);

    if (!pipeline || !src || !srccapsfilter || !timeoverlay || !enc || !enc_queue || !sink)
    {
        g_print("FAILED to create common element.\n");
        return -1;
    }
    else
    {
        g_print("All common elements are created.\n");
    }

    //Build the pipeline. Note that we are NOT linking.
    gst_bin_add_many(GST_BIN(pipeline), src, srccapsfilter, timeoverlay, enc, enc_queue, sink, NULL);

    //Set the properties of the object
    srcCaps  = gst_caps_new_simple ("video/x-raw",
                                "width",     G_TYPE_INT,        1920,
                                "height",    G_TYPE_INT,        1080,
                                "framerate", GST_TYPE_FRACTION, 30, 1,
                                 NULL);
    g_object_set (G_OBJECT(srccapsfilter), "caps", srcCaps, NULL);
    gst_caps_unref (srcCaps);

    g_object_set(G_OBJECT(timeoverlay), "time-mode", 2, NULL);
    g_object_set(G_OBJECT(enc), "bframes", 0, NULL);
    g_object_set(G_OBJECT(sink), "emit-signals", TRUE, "sync", FALSE, NULL);
    g_signal_connect(sink, "new-sample", G_CALLBACK(on_new_sample_from_sink), NULL);
    
    //link the pipeline
    gst_element_link_many(src, srccapsfilter, timeoverlay, enc,enc_queue, sink, NULL);

    //add message monitor
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_watch(bus, bus_callback, loop);
    gst_object_unref(bus);

    //start the pipeline
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    if (GST_STATE_CHANGE_FAILURE == ret)
    {
        g_print("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }
    else
    {
        g_print("start the pipeline......\n");
    }

    //start the loop
    g_main_loop_run(loop);

    // Free resources
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_print("stop the pipeline.\n");

    return 0;
}
