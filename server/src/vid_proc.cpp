#include "vid_proc.h"
#include "gst_meta.h"

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
		{
			std::lock_guard<std::mutex> lock (cb_dat->lock_q);
			gst_sample_ref (sample);
			cb_dat->frame_q.push (sample);
		}
		emit cb_dat->vid_proc_ptr->new_frame_sig ();
		gst_sample_unref (sample);
		return GST_FLOW_OK;
	}
	printf("GST_ERROR\n");
	gst_sample_unref (sample);
	return GST_FLOW_ERROR;
}

static void print_hex(const gchar *label, const guint8 *data, gsize size) {
    g_print("%s [%zu bytes]:\n", label, size);
    for (gsize i = 0; i < size; ++i) {
        g_print("%02X ", data[i]);
        if ((i + 1) % 16 == 0) g_print("\n");
    }
    if (size % 16 != 0) g_print("\n");
}

GstPadProbeReturn rtpdepay_probe_callback (GstPad* pad, GstPadProbeInfo* info, gpointer user_dat) {
	Probe_Data* probe_data_ptr = static_cast<Probe_Data*>(user_dat);
	GstBuffer* buffer;
	gsize size;
	std::vector <std::vector <double>>::iterator it;
	std::vector <double> newPoint;

	// Update time now (received time)
	auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
	uint64_t ts_now = now_us.time_since_epoch().count();
    uint64_t ts_sent = 0;
	
	if (!(GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER)) return GST_PAD_PROBE_OK;

	buffer = GST_PAD_PROBE_INFO_BUFFER (info);
	if (!buffer) return GST_PAD_PROBE_OK;

	// Update bitrate information
	guint buf_size = gst_buffer_get_size (buffer);

	probe_data_ptr->lock_pd.lock ();
	
	it = probe_data_ptr->dat_buf->begin ();
	newPoint.insert (newPoint.begin (), buf_size*8);
	//newPoint.insert (newPoint.begin (), ts_cur);
	probe_data_ptr->dat_buf->insert (it, newPoint);
	probe_data_ptr->num_recv_bits += buf_size * 8;

	probe_data_ptr->lock_pd.unlock ();

	// Monitor sequence numbers
	GstRTPBuffer rtp = {0};
	if (!gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp)) return GST_PAD_PROBE_OK;
	guint16 seqnum = gst_rtp_buffer_get_seq (&rtp);
	gst_rtp_buffer_unmap (&rtp);

	// Get buffer and error check
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) return GST_PAD_PROBE_OK;

    const guint8 *data = map.data;
    size = map.size;

    if (size < 12) {
        gst_buffer_unmap(buffer, &map);
        return GST_PAD_PROBE_OK;
    }

    guint8 version = (data[0] >> 6) & 0x03;
    if (version != 2) {
        gst_buffer_unmap(buffer, &map);
        return GST_PAD_PROBE_OK;
    }

    guint8 has_ext = (data[0] & 0x10);
    guint8 csrc_count = data[0] & 0x0F;
    guint header_len = 12 + 4 * csrc_count;

    if (has_ext) {
        //gst_buffer_unmap(buffer, &map);
        //g_print("Receiver: No RTP extension bit set.\n");

        if (size < header_len + 4) {
            gst_buffer_unmap(buffer, &map);
            //g_print("Receiver: Not enough data for extension header.\n");
            return GST_PAD_PROBE_OK;
        }

        const guint8 *ext_hdr = data + header_len;
        guint16 profile = (ext_hdr[0] << 8) | ext_hdr[1];
        guint16 length_words = (ext_hdr[2] << 8) | ext_hdr[3];  // In 32-bit words
        guint ext_len = length_words * 4;

        if (profile != 0xBEDE) {
            gst_buffer_unmap(buffer, &map);
            //g_print("Receiver: Unexpected extension profile: 0x%04X\n", profile);
            return GST_PAD_PROBE_OK;
        }

        if (size < header_len + 4 + ext_len) {
            gst_buffer_unmap(buffer, &map);
            //g_print("Receiver: Extension claims too much data (corrupt?)\n");
            return GST_PAD_PROBE_OK;
        }

        const guint8 *ext_data = ext_hdr + 4;
        //print_hex("Receiver: RTP Extension Raw Data", ext_data, ext_len);

        // Read embedded time sent (sending timestamp)
        guint offset = 0;
        while (offset < ext_len) {
            guint8 b = ext_data[offset];
            if (b == 0x00) {
                offset++; // padding byte
                continue;
            }

            guint8 ext_id = b >> 4;
            guint8 len = (b & 0x0F) + 1;
            offset++;

            if (offset + len > ext_len) {
                //g_debug("Receiver: Skipping invalid or truncated extension (ID=%u, len=%u)", ext_id, len);
                break;
            }

            if (ext_id == (guint8)1 && len == 8) {
                memcpy(&ts_sent, &ext_data[offset], 8);
            } else {
                //g_print("Receiver: Unknown Extension ID %u, length %u\n", ext_id, len);
                break;
            }
            offset += len;
        }
    }
    gst_buffer_unmap(buffer, &map);
    printf ("%u %ld %ld %d %ld\n", seqnum, ts_sent, ts_now, buf_size * 8, ts_now - ts_sent);

	return GST_PAD_PROBE_OK;
}

Video_Proc::Video_Proc (QObject* parent, char* uri, int type, GstElement* consumer, std::vector<std::vector<double>> *dat_buf) : QThread (parent) {
	//std::vector<std::vector<double>> *probe_dat;
	this->probe_dat.dat_buf = dat_buf;
	this->type = type;

	pipeline = gst_parse_launch (uri, NULL);
	if (!pipeline) {
		fprintf (stderr, "Failed to create pipeline. Exiting\n");
		return;	
	}

	this->appsink = gst_bin_get_by_name (GST_BIN (pipeline), "appsink");
	this->src = gst_bin_get_by_name (GST_BIN (pipeline), "udpsrc");
	GstElement* depay = gst_bin_get_by_name (GST_BIN (pipeline), "depay");
	if (!appsink || !src) {
		fprintf (stderr, "Failed to retreive appsink or udpsrc. Exiting\n");
		return;
	}

	// configure udpsrc pad probe
	GstPad* pad = gst_element_get_static_pad (depay, "sink");
	this->probe_dat.pipeline = pipeline;
	gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, rtpdepay_probe_callback, &this->probe_dat.dat_buf, NULL);
	gst_object_unref (pad);

	// configure rtp pad probe
	//pad = gst_element_get_static_pad (depay_elem, "sink");
	//gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, depaysink_probe_callback, NULL, NULL);
	//gst_object_unref (pad);

	// Configure appsink new sample callback
	cb_dat.callback = {nullptr, nullptr, new_sample_callback, nullptr};
	cb_dat.vid_proc_ptr = this;
	gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &cb_dat.callback, &cb_dat, nullptr);

	// Configure appsink sink pad probe
	pad = gst_element_get_static_pad (appsink, "sink");
	//gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, appsink_probe_callback, NULL, NULL);
	gst_object_unref (pad);

	printf ("VIDEO PROCESSOR (%s) PIPELINE CONFIGURED\n", (type == 0) ? "READER" : "WRITER");

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
