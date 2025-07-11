#ifndef VID_PROC_H
#define VID_PROC_H

#include <QObject>
#include <QThread>

#include <opencv2/opencv.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <gst/rtp/gstrtpbuffer.h>

#include <thread>
#include <unistd.h>
#include <X11/Xlib.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

class Video_Proc;

// Used to pass data to gstreamer
struct Callback_Data {
	GstAppSinkCallbacks callback;
	Video_Proc* vid_proc_ptr;
	GMainLoop* loop;
	GstElement* writer_src_ref;
	int Fcnt;
    std::chrono::steady_clock::time_point Tbegin;
	float FPS [16];
	std::queue<GstSample*> frame_q;
	std::mutex lock_q;
};

struct Probe_Data {
	std::vector<std::vector<double>> *dat_buf;
    int num_recv_bits = 0;
	std::mutex lock_pd;
    GstElement* pipeline;
};

// Gstreamer wrapper object
class Video_Proc : public QThread {
	Q_OBJECT
public:
	bool pipe_running;
	Probe_Data probe_dat;

	Video_Proc (QObject* parent=nullptr, char* uri=NULL, int type=0, GstElement* consumer=nullptr, std::vector<std::vector<double>>* dat_buf=nullptr);
	~Video_Proc () {
		printf ("VID_PROC DESTROYED\n");
		emit finished ();
	}
	GstElement* get_src () {return src;}
	Callback_Data cb_dat;

public slots:
	void set_term_sig (int var) {
		term_sig = var;
	}
	void set_toggle_sig (int var) {
		toggle_sig = var;
	}

signals: 
	void new_latency (const float);
	void new_frame_sig ();
	void finished();

private:

	int type;
	GstElement* pipeline;
	GMainLoop* loop;
	GstBus* bus;
	GstElement* src;
	GstElement* appsink;
	GstElement* udpsink;
	int term_sig;
	int toggle_sig;
	bool paused;


	static gboolean int_src_cb (gpointer);

protected:
	void run () override;
};
#endif









