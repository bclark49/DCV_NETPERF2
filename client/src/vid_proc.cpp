#include "vid_proc.h"

extern const int WIDTH;
extern const int HEIGHT;
int num_in_buffers = 0;
double data_before = 0;
double data_after = 0;

static void fps_callback (GstElement* sink, gdouble fps, gdouble droprate, gdouble avgfps, gpointer dat) {
	guint64 num_frames;
	g_object_get (sink, "frames_rendered", &num_frames, NULL);
	//printf ("CALLED(%d): %f, %f, %f\n", num_frames, fps, droprate, avgfps);
	//const GValue* fps = gst_structure_get_value (s, "fps");
	/*
	if (fps) {
		double fps_val = g_value_get_double (fps);
		printf ("FPS_SINK: %f\n", fps_val);
	}
	*/
}

gboolean bus_callback (GstBus* bus, GstMessage* message, gpointer data) {
	GMainLoop* loop = (GMainLoop*) data;

	switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_EOS: {
			g_print ("GST THREAD FOUND EOS\n");
			g_main_loop_quit (loop);
			break;
		}
		case GST_MESSAGE_ERROR: {
			gchar* debug;
			GError* error;
			gst_message_parse_error (message, &error, &debug);
			g_printerr ("ERROR GST THREAD: %s\n", error->message);
			g_error_free (error);
			g_free (debug);
		
			g_main_loop_quit (loop);
			break;
		}
		default:
			break;
	}
	return true;	
}

// This function is called when a sample is available
// sink: appsink from gstreamer pipe
// data: holds object for emitting frame to app
// return: error for gstreamer
GstFlowReturn new_sample_callback (GstAppSink* sink, gpointer data) {
	// Convert GST sample to Opencv Mat
	GstSample* sample = gst_app_sink_pull_sample (GST_APP_SINK(sink));
	Callback_Data* cb_dat = static_cast<Callback_Data*>(data);
	if (sample != nullptr) {
		GstBuffer* buffer = gst_sample_get_buffer (sample);
		if (buffer != nullptr) {
			GstMapInfo map;
			if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
				//printf ("WIDTHxHEIGHT: %dx%d\n", WIDTH, HEIGHT);
				cv::Mat m (cv::Size (WIDTH, HEIGHT), CV_8UC3, map.data);
				guint size = m.total () * m.elemSize ();	
				emit cb_dat->vid_proc_ptr->new_frame_sig (m.clone ());

				gst_buffer_unmap (buffer, &map);
				gst_sample_unref (sample);
				return GST_FLOW_OK;
			}
			gst_buffer_unmap (buffer, &map);
		}
		gst_sample_unref (sample);
	}
	printf("GST_ERROR\n");
	return GST_FLOW_ERROR;
}

GstPadProbeReturn probe_callback (GstPad* pad, GstPadProbeInfo* info, gpointer user_dat) {
    std::chrono::high_resolution_clock::time_point tp_cur;
    tp_cur = std::chrono::high_resolution_clock::now ();
    //int tp_size = sizeof (std::chrono::high_resolution_clock::time_point);
    int ts_size = sizeof (long int);

    // Grab buffer for writing, extract info
	if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER) {
        GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER (info);

        // Map buffer
        if (buffer) {
            GstMapInfo map;
            buffer = gst_buffer_make_writable (buffer);
            GST_PAD_PROBE_INFO_DATA (info) = buffer; 
            gst_buffer_map (buffer, &map, GST_MAP_READWRITE);

            guint size = gst_buffer_get_size (buffer);


        }
    }
	return GST_PAD_PROBE_OK;
}

int counter = 0;

void print_hex(const gchar *label, const guint8 *data, gsize size) {
    g_print("%s [%zu bytes]:\n", label, size);
    for (gsize i = 0; i < size; ++i) {
        g_print("%02X ", data[i]);
        if ((i + 1) % 16 == 0) g_print("\n");
    }
    if (size % 16 != 0) g_print("\n");
}

GstPadProbeReturn rtppay_pad_probe (GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    if (!(GST_PAD_PROBE_INFO_TYPE(info) && GST_PAD_PROBE_TYPE_BUFFER)) return GST_PAD_PROBE_OK;

    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_OK;
    auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
    uint64_t ts_now;

    GstRTPBuffer rtp = {0};
    if (!gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp)) return GST_PAD_PROBE_OK;
    guint16 seqnum = gst_rtp_buffer_get_seq (&rtp);
    gst_rtp_buffer_unmap (&rtp);

    if (counter % 100 == 0) {
        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) return GST_PAD_PROBE_OK;

        guint8 *data = map.data;
        gsize size = map.size;

        if (size < 12) {
            gst_buffer_unmap(buffer, &map);
            return GST_PAD_PROBE_OK;
        }

        guint8 version = (data[0] >> 6) & 0x03;
        if (version != 2) {
            gst_buffer_unmap(buffer, &map);
            return GST_PAD_PROBE_OK;
        }

        guint8 csrc_count = data[0] & 0x0F;
        guint header_len = 12 + csrc_count * 4;

        if (size < header_len) {
            gst_buffer_unmap(buffer, &map);
            return GST_PAD_PROBE_OK;
        }

        // Create extension data
        guint8 ext_id = 1;
        guint8 ts_len = 8;
        guint8 ext_header_len = 1 + ts_len;
        guint16 ext_len_words = (ext_header_len + 3) / 4;
        guint ext_data_len = ext_len_words * 4;

        guint ext_total_len = 4 + ext_data_len; // 4-byte extension header + extension block

        guint new_size = size + ext_total_len;
        GstBuffer *new_buf = gst_buffer_new_allocate(NULL, new_size, NULL);
        GstMapInfo new_map;
        gst_buffer_map(new_buf, &new_map, GST_MAP_WRITE);

        // Copy over PTS/DTS/offset/timestamps/etc.
        GST_BUFFER_PTS(new_buf) = GST_BUFFER_PTS(buffer);
        GST_BUFFER_DTS(new_buf) = GST_BUFFER_DTS(buffer);
        GST_BUFFER_OFFSET(new_buf) = GST_BUFFER_OFFSET(buffer);
        GST_BUFFER_DURATION(new_buf) = GST_BUFFER_DURATION(buffer);
        gst_buffer_copy_into(new_buf, buffer, static_cast <GstBufferCopyFlags> (GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_META), 0, -1);

        // Copy RTP header
        memcpy(new_map.data, data, header_len);

        // Set extension bit
        new_map.data[0] |= 0x10;

        // Write extension header
        new_map.data[header_len + 0] = 0xBE;
        new_map.data[header_len + 1] = 0xDE;
        new_map.data[header_len + 2] = 0x00;
        new_map.data[header_len + 3] = ext_len_words;

        // Write extension entry
        now_us = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
        ts_now = now_us.time_since_epoch().count();
        new_map.data[header_len + 4] = (ext_id << 4) | (ts_len - 1);
        memcpy(&new_map.data[header_len + 5], &ts_now, ts_len);

        // Copy original payload
        memcpy(new_map.data + header_len + ext_total_len, data + header_len, size - header_len);

        gst_buffer_unmap(buffer, &map);
        gst_buffer_unmap(new_buf, &new_map);

        // Replace buffer in probe info
        GST_PAD_PROBE_INFO_DATA(info) = new_buf;
        gst_buffer_unref(buffer); // release old
        buffer = new_buf;
    }

    //if (!gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp)) return GST_PAD_PROBE_OK;
    now_us = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
    ts_now = now_us.time_since_epoch().count();
    printf ("%u %ld %ld\n", seqnum, ts_now, gst_buffer_get_size (buffer) * 8);
    counter++;
    return GST_PAD_PROBE_OK;
}

Video_Proc::Video_Proc (QObject* parent, char* uri, int type, GstElement* consumer, std::vector<std::vector<double>> *LatencyBuff) : QThread (parent) {
	this->type = type;
	pipeline = gst_parse_launch (uri, NULL);
	if (!pipeline) {
		fprintf (stderr, "Failed to create pipeline. Exiting\n");
		return;	
	}	
    //GstElement* cam_src = gst_bin_get_by_name (GST_BIN (pipeline), "v4l2src");
    GstElement* rtppay = gst_bin_get_by_name (GST_BIN (pipeline), "pay0");
    GstElement* udpsink = gst_bin_get_by_name (GST_BIN (pipeline), "udpsink");
	// Configure udpsink pad probe
    GstPad* pad = gst_element_get_static_pad (udpsink, "sink");
    gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, rtppay_pad_probe, pipeline, NULL);
    gst_object_unref (pad);

	loop = g_main_loop_new (nullptr, false);

	bus = gst_element_get_bus (pipeline);
	gst_bus_add_watch (bus, bus_callback, loop);
}

// Seperate class for gstreamer. only runs in this thread
void Video_Proc::run () {
	g_timeout_add (100, reinterpret_cast<GSourceFunc>(&Video_Proc::int_src_cb), this);
	if (gst_element_set_state (pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
		fprintf (stderr, "Failed to set state to playing. Exiting\n");
		gst_object_unref (pipeline);
		return;
	}

	// Set pipeline managment variables
	term_sig = false;
	toggle_sig = false;
	paused = false;
	pipe_running = true;
	// Starting running pipeline
	printf ("VIDEO PROCESSOR (%s) RUNNING\n", (type == 0) ? "READER" : "WRITER");
	g_main_loop_run (loop);
	pipe_running = false;
	printf ("VIDEO PROCESSOR (%s) EXIT LOOP\n", (type == 0) ? "READER" : "WRITER");

	// Clean up after finished
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (bus);
	gst_object_unref (GST_OBJECT(pipeline));
	g_main_loop_unref (loop);
	return;
}

// Function will interupt pipeline periodically to kill, pause, or play execution
gboolean Video_Proc::int_src_cb (gpointer data) {
	Video_Proc* vid_proc_ptr = static_cast<Video_Proc*>(data);
	if (vid_proc_ptr->term_sig == 1) {
		g_main_loop_quit (vid_proc_ptr->loop);
		return G_SOURCE_REMOVE;
	}
	else if (vid_proc_ptr->toggle_sig == 1) {
		if (vid_proc_ptr->paused == 1) {
			if (gst_element_set_state (vid_proc_ptr->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
				fprintf (stderr, "Failed to set state to playing");
				g_main_loop_quit (vid_proc_ptr->loop);
				return G_SOURCE_REMOVE;
			}
			vid_proc_ptr->paused = 0;
		}
		else {
			if (gst_element_set_state (vid_proc_ptr->pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE){
				fprintf (stderr, "Failed to set state to paused");
				g_main_loop_quit (vid_proc_ptr->loop);
				return G_SOURCE_REMOVE;
			}
			vid_proc_ptr->paused = 1;
		}
		vid_proc_ptr->set_toggle_sig (0);
	}
	return G_SOURCE_CONTINUE;
}
