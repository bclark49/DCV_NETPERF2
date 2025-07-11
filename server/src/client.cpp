#include "mainwindow.h"

int WIDTH; 
int HEIGHT; 

int main (int argc, char** argv) {
	char host_ip [32], device[16];
	int width, height, fps;
	char debug[16];

	// Initialize Gstreamer
	gst_init (&argc, &argv);
	GstDebugLevel dgblevel = gst_debug_get_default_threshold ();

	if (argc == 2) {
		strcpy (debug, argv[1]);	
		if (strcmp(debug, "INFO") == 0) dgblevel = GST_LEVEL_INFO;
		else if (strcmp(debug, "ERROR") == 0) dgblevel = GST_LEVEL_ERROR;
		else {
			fprintf (stderr, "INPUT ERROR: debug_level string incorrect.\n");
			return -1;
		}
	}
	else if (argc == 1) {
		dgblevel = GST_LEVEL_ERROR;
	}
	else {
		fprintf (stderr, "usage: ./app [debug_level (INFO|ERROR)]\n");
		return -1;
	}

    /*
	width = atoi (argv[2]), height = atoi (argv[3]);
	WIDTH = width, HEIGHT = height;
	fps = atoi (argv[4]);
	strcpy (device, argv[1]);
	strcpy (host_ip, argv[5]);
    */

	// Set Gstreamer debug level
	if (!gst_debug_is_active () ) {
		gst_debug_set_active (true);
		gst_debug_set_default_threshold (dgblevel);
	}

	//int buffer_size = (WIDTH * HEIGHT * 3);
	//int max_buffers = 60;
	//int max_ns = 1000;

	// Start video reader/writer
	char reader_uri [1024];
	std::vector<std::vector<double>> dataBEFORE;

	//sprintf (reader_uri, "udpsrc port=9002 name=udpsrc caps=\"application/x-rtp, media=video, clock-rate=90000, encoding-name=H265, alignment=au, payload=96\" ! rtph265depay ! video/x-h265, stream-format=byte-stream, alignment=au ! queue max-size-buffers=%d ! h265parse config-interval=1 ! queue max-size-buffers=%d ! avdec_h265 qos=true ! video/x-raw, width=%d, height=%d, framerate=%d/1, format=BGR ! appsink name=appsink blocksize=%d max-buffers=%d max-lateness=%d qos=true sync=false", 
			//max_buffers, max_buffers, width, height, fps, buffer_size, max_buffers, max_ns);
			//
    /*
	sprintf (reader_uri, "udpsrc name=udpsrc port=9002 ! application/x-rtp, media=video, encoding-name=H265 ! rtph265depay ! queue !  avdec_h265 ! video/x-raw, width=%d, height=%d, framerate=%d/1, format=YUY2 ! queue ! appsink name=appsink qos=true sync=false", width, height, fps);
    */
	//sprintf (reader_uri, "udpsrc name=udpsrc port=9002 ! application/x-rtp, media=video, encoding-name=H265 ! rtph265depay ! queue !  avdec_h265 ! queue ! appsink name=appsink qos=true sync=false");
	sprintf (reader_uri, "udpsrc name=udpsrc port=9002 ! application/x-rtp, media=video, encoding-name=H264 ! rtph264depay name=depay ! avdec_h264 qos=true ! appsink name=appsink qos=true sync=false");

	/*
	sprintf (reader_uri, "v4l2src device=%s blocksize=%d name=v4l2src ! video/x-raw, width=%d, height=%d, framerate=%d/1, format=BGR ! timeoverlay ! tee name=t \
			t. ! queue max-size-buffers=%d ! mpph265enc qos=true ! video/x-h265, stream-format=byte-stream, alignment=au ! queue max-size-buffers=%d ! rtph265pay name=pay0 config-interval=1 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H265, payload=96 ! queue max-size-buffers=%d ! udpsink name=udpsink host=%s port=9002 qos=true max-lateness=%d sync=false \
			t. ! queue max-size-buffers=%d ! appsink name=appsink blocksize=%d max-buffers=%d max-lateness=%d qos=true sync=false", 
			device, buffer_size, width, height, fps, max_buffers, max_buffers, max_buffers, host_ip, max_ns, max_buffers, buffer_size, max_buffers, max_ns);
	*/

	// Configure QT application and start QT thread
	qRegisterMetaType<cv::Mat>("cv::Mat&");
	QApplication app (argc, argv);
	MainWindow w (nullptr, reader_uri, &dataBEFORE);
	printf ("MAINWINDOW INITIALIZED\n");
	w.show();
	printf ("SHOWING WINDOW\n");

	app.exec();
	printf ("APP RETURNED\n");

    return 0;
}
