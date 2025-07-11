#include "mainwindow.h"

int WIDTH; 
int HEIGHT; 

int main (int argc, char** argv) {
	char host_ip [32], device[32], format[8];
	int width, height, fps;
	char debug[16];
	bool filesrc;
	host_ip[0] = '\0', device[0] = '\0', format[0] = '\0';

	// Initialize Gstreamer
	gst_init (&argc, &argv);
	GstDebugLevel dgblevel = gst_debug_get_default_threshold ();

	if (argc == 8) {		// Full arguments
		// Check debug level
		if (strlen(argv[7]) > 16) {
			fprintf (stderr, "INPUT ERROR: debug_level character limit(16) surpassed.\n");
			return -1;
		}
		strcpy (debug, argv[7]);	
		if (strcmp(debug, "INFO") == 0) dgblevel = GST_LEVEL_INFO;
		else if (strcmp(debug, "ERROR") == 0) dgblevel = GST_LEVEL_ERROR;
		else {
			fprintf (stderr, "INPUT ERROR: debug_level string incorrect.\n");
			return -1;
		}

		// Check format
		if (strlen(argv[5]) > 8) {
			fprintf (stderr, "INPUT ERROR: format character limit(8) surpassed.\n");
			return -1;
		}
		strcpy (format, argv[5]);	
		if ((strcmp (format, "BGR") != 0) && (strcmp (format, "NV12") != 0) && (strcmp (format, "YUY2") != 0)) {
			fprintf (stderr, "INPUT ERROR: format string incorrect.\n");
			return -1;
		}
		filesrc = false;
		strcpy (host_ip, argv[6]);
	}
	else if (argc == 7) { 	// Format omitted (must be filesrc)
		// Check debug level
		if (strlen(argv[6]) > 16) {
			fprintf (stderr, "INPUT ERROR: debug_level character limit(16) surpassed.\n");
			return -1;
		}
		strcpy (debug, argv[6]);	
		if (strcmp(debug, "INFO") == 0) dgblevel = GST_LEVEL_INFO;
		else if (strcmp(debug, "ERROR") == 0) dgblevel = GST_LEVEL_ERROR;
		else {
			fprintf (stderr, "INPUT ERROR: debug_level string incorrect.\n");
			return -1;
		}
		printf ("FILESRC TRUE\n");
		filesrc = true;
		strcpy (host_ip, argv[5]);
	}
	else {
		fprintf (stderr, "usage: ./app device width height fps [format(BGR|NV12)] host_ip [debug_level(INFO|ERROR)]\n");
		return -1;
	}

	width = atoi (argv[2]), height = atoi (argv[3]);
	WIDTH = width, HEIGHT = height;
	fps = atoi (argv[4]);
	strcpy (device, argv[1]);

	// Set Gstreamer debug level
	if (!gst_debug_is_active () ) {
		gst_debug_set_active (true);
		gst_debug_set_default_threshold (dgblevel);
	}

	int buffer_size = (WIDTH * HEIGHT * 3);
	int max_buffers = 60;
	int max_ns = 1000;

	// Start video reader/writer
	char reader_uri [1024];
	char writer_uri [1024];
	std::vector<std::vector<double>> dataBEFORE;
    std::vector<std::vector<double>> dataAFTER;

	if (!filesrc) {
		/*sprintf (reader_uri, "v4l2src device=%s name=v4l2src ! video/x-raw, width=%d, height=%d, framerate=%d/1, format=%s ! timeoverlay auto-resize=false color=4278190080 outline-color=4278190080 draw-shadow=false shaded-background=true shading-value=255 font-desc=\"Monospace Normal Bold 100\" ! tee name=t \
         */
        /*
		sprintf (reader_uri, "v4l2src device=%s name=v4l2src ! video/x-raw, width=%d, height=%d, framerate=%d/1, format=%s ! timeoverlay auto-resize=false draw-shadow=false shaded-background=true shading-value=255 font-desc=\"Monospace Normal Bold 100\" ! tee name=t \
				t. ! queue ! nvvidconv qos=true ! nvv4l2h265enc qos=true ! video/x-h265, stream-format=byte-stream, alignment=au ! queue ! rtph265pay name=pay0 config-interval=1 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H265, payload=96 ! queue ! udpsink name=udpsink host=%s port=9002 qos=true sync=true \
				t. ! queue ! fpsdisplaysink sink=\"xvimagesink\" name=appsink sync=true", 
				device, width, height, fps, format, host_ip);
		sprintf (reader_uri, "v4l2src device=%s name=v4l2src ! video/x-raw, width=%d, height=%d, framerate=%d/1, format=%s ! queue name=q1 ! timeoverlay name=timeoverlay auto-resize=false draw-shadow=false shaded-background=true shading-value=255 font-desc=\"Monospace Normal Bold 100\" ! tee name=t \
				t. ! queue name=q2 ! nvvidconv qos=true ! nvv4l2h264enc qos=true ! video/x-h264, stream-format=byte-stream, alignment=au ! queue ! rtph264pay name=pay0 config-interval=1 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96 ! queue ! udpsink name=udpsink host=%s port=9002 qos=true sync=true \
				t. ! queue ! fpsdisplaysink sink=\"xvimagesink\" name=appsink sync=true", 
				device, width, height, fps, format, host_ip);
        */

		sprintf (reader_uri, "v4l2src device=%s name=v4l2src ! video/x-raw, width=%d, height=%d, framerate=%d/1, format=%s  ! timeoverlay name=timeoverlay auto-resize=false draw-shadow=false shaded-background=true shading-value=255 font-desc=\"Monospace Normal Bold 100\" ! tee name=t \
				t. ! queue name=q2 ! nvvidconv qos=true ! video/x-raw(memory:NVMM), format=NV12 ! nvv4l2h264enc qos=true ! video/x-h264, stream-format=byte-stream, alignment=au ! rtph264pay name=pay0 config-interval=1 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H264, payload=96 ! udpsink name=udpsink host=%s port=9002 sync=false qos=true \
				t. ! queue ! fpsdisplaysink sink=\"xvimagesink\" name=appsink sync=false", 
				device, width, height, fps, format, host_ip);
	}
	else{
		sprintf (reader_uri, "filesrc location=%s name=v4l2src ! qtdemux ! queue ! h264parse ! queue ! avdec_h264 ! timeoverlay ! tee name=t \ 
				t. ! queue ! nvvidconv ! nvv4l2h265enc qos=true ! video/x-h265, stream-format=byte-stream, alignment=au ! queue ! rtph265pay name=pay0 config-interval=1 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H265, payload=96 ! queue ! udpsink name=udpsink host=%s port=9002 qos=true sync=true \
				t. ! queue ! xvimagesink name=appsink sync=true", 
				device, host_ip);
		/*
		sprintf (reader_uri, "filesrc location=%s name=v4l2src ! qtdemux ! queue ! h264parse ! queue ! mppvideodec format=16 qos=true ignore-error=true ! timeoverlay ! queue ! kmssink name=appsink sync=false", 
				device, host_ip);
		sprintf (reader_uri, "filesrc location=%s name=v4l2src ! qtdemux ! queue ! h264parse ! queue ! mppvideodec format=16 qos=true ignore-error=true ! timeoverlay ! tee name=t \ 
				t. ! queue ! mpph265enc qos=true ! video/x-h265, stream-format=byte-stream, alignment=au ! queue ! rtph265pay name=pay0 config-interval=1 ! application/x-rtp, media=video, clock-rate=90000, encoding-name=H265, payload=96 ! queue ! udpsink name=udpsink host=%s port=9002 qos=true sync=true \
				t. ! queue ! kmssink name=appsink sync=false", 
				device, host_ip);
		*/
	}

	/*
	sprintf (writer_uri, "udpsrc port=9004 name=udpsrc caps=\"application/x-rtp, media=video, clock-rate=90000, encoding-name=H265, alignment=au, payload=96\" ! rtph265depay ! video/x-h265, stream-format=byte-stream, alignment=au ! queue max-size-buffers=%d ! h265parse config-interval=1 ! queue ! mppvideodec format=16 ! video/x-raw, width=%d, height=%d, format=BGR, framerate=%d/1 ! appsink name=appsink qos=true sync=true", max_buffers, width, height, fps);
	*/

	// Configure QT application and start QT thread
	qRegisterMetaType<cv::Mat>("cv::Mat");
	QApplication app (argc, argv);
	MainWindow w (nullptr, NULL, reader_uri, &dataBEFORE, &dataAFTER );
	printf ("MAINWINDOW INITIALIZED\n");
	//w.show();
	printf ("SHOWING WINDOW\n");

	app.exec();
	printf ("APP RETURNED\n");

    return 0;
}
