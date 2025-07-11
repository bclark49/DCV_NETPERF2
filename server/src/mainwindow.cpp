#include "mainwindow.h"
#include "ui_mainwindow.h"

// Globals set by main, needed for gst -> opencv conversion and display
extern const int WIDTH;
extern const int HEIGHT;

//int before_count = 0;
//int after_count = 0;
int pack_num = 0;

// Calculates and draws framerate
// m: QImage to draw on (passed by reference
// Tbegin/Tend: start and stop time of a frame
// Fcnt: frame count
// FPS: holds 16 durations for averaging
void annotate_frame (QImage &m, std::chrono::steady_clock::time_point Tbegin, 
			std::chrono::steady_clock::time_point Tend, 
			int &Fcnt, float (&FPS)[16]) {
	char str [64];
	float f;
	int i;
	f = std::chrono::duration_cast <std::chrono::milliseconds> (Tend-Tbegin).count ();
	if (f>0.0) FPS[((Fcnt++)&0x0F)] = 1000.0 / f;
	for (f=0.0, i=0; i<16; i++) {f+=FPS[i];}
	sprintf (str, "FPS: %0.2f, Resolution: %d/%d", f/16, WIDTH, HEIGHT);
	QPainter painter (&m);
	QPen pen (Qt::green, 32, Qt::SolidLine);
	painter.setPen (pen);
	painter.setFont (QFont ("Courier New", 24, QFont::Bold));
	painter.drawText (QPoint (100,200), str);
}

// Finds 0.5s interval of packet samples from vector
// packet_buf: Vector to store size and timestamp samples from video processors 
// ts_start: Timestamp for the first packet sent 
// total: running total of time for the stream
// return: the timestamp of the end of a 0.5s interval
double find_bitrate (std::vector<std::vector <double>>* pkt_bufA, long int ts_start, int update_count) {

	double tsA_curr = 0;
	double tsA_prev = 0;
	int run_tot = 0;

	std::vector<std::vector <double>>::iterator itA;

	itA = pkt_bufA->end ();
	itA--;
	tsA_prev = (*itA)[0];
	

	for (itA=pkt_bufA->begin(); itA<(pkt_bufA->end());itA++){
		run_tot += (*itA)[1];
	}
	itA = pkt_bufA->begin ();
	tsA_curr = (*itA)[0];
	pkt_bufA->erase (itA, pkt_bufA->end());
	//printf("tsA_curr,tsA_prev,difference %f / %f / %fus\n",tsA_curr,tsA_prev,tsA_curr-tsA_prev);
	return run_tot;
}

// Constructor: inherits QMainWindow
MainWindow::MainWindow (QWidget* parent, char* cam_src_str, std::vector<std::vector<double>> *dataBEFORE) : QMainWindow(parent), ui(new Ui::MainWindow) {
	// Set up UI
	ui->setupUi(this);
	setupUI();
	
	cam_src_ptr = new Video_Proc (this, cam_src_str, 0, NULL, dataBEFORE);

	// Connect signals for producer pipeline
	connect (cam_src_ptr, &Video_Proc::finished, cam_src_ptr, &Video_Proc::deleteLater);
	connect (cam_src_ptr, &Video_Proc::finished, cam_src_ptr, &Video_Proc::quit);
	connect (cam_src_ptr, &Video_Proc::new_frame_sig, this, &MainWindow::updateFrameBefore);
	connect (this, &MainWindow::stop_vid_proc, cam_src_ptr, &Video_Proc::set_term_sig);

	// Set up for FPS measurements
	for (int i=0; i<16; i++) camsrc_FPS[i] = 0.0;
	camsrc_Fcnt = 0;
	TP_camsrc_start = std::chrono::steady_clock::now ();
	cam_src_ptr->start ();

	// To manipulate pipelines
	connect (ui->startButton, &QPushButton::clicked, this, &MainWindow::toggleVideo);
	connect (ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopVideo);

	scene = new QGraphicsScene(this);
	ui->graph->setScene(scene);

	// Initialize path and pen for the graph line
	// Set up initial graph properties
	this->datab_ptr=dataBEFORE;

	red.setWidth(2);
	dark_red.setWidth(2);
	blue.setWidth(2);
	dark_blue.setWidth(2);

	// Create axes
	createAxis(0);

	// Start the timer to update the graph
	timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &MainWindow::updateGraph);
	timer->start(1000); // Update every 1 seconds
}

// Free resources
MainWindow::~MainWindow () {
	printf("DESTROY MAINWINDOW\n");
	delete cam_src_ptr;
	//delete udp_src_ptr;
	delete ui;
}

void MainWindow::setupUI () {
	printf ("SETTING UP UI\n");
    //setWindowState(Qt::WindowFullScreen);
	ui->label1->setFixedSize(960, 720);
	ui->label2->setFixedSize(960, 720);
	ui->label1->setAlignment((Qt::AlignmentFlag)(Qt::AlignLeft & Qt::AlignTop));
	ui->label2->setAlignment((Qt::AlignmentFlag)(Qt::AlignLeft & Qt::AlignTop));
}

// Called when window is closed. Used to syncronize exit
void MainWindow::closeEvent (QCloseEvent *event) {
	//if (cam_src_ptr->pipe_running || udp_src_ptr->pipe_running) {
	if (cam_src_ptr->pipe_running) {
		QMessageBox popup;
		popup.setWindowTitle ("STOP VIDEO!");
		popup.setText ("Please press stop before closing the main window.");
		popup.exec ();
		event->ignore ();
	}
	else {
		cam_src_ptr->wait ();
		//udp_src_ptr->wait ();
		event->accept ();
	}
}

// Called when esc key pressed
void MainWindow::keyPressEvent (QKeyEvent *event) {
	if (event->key() == Qt::Key_Escape) {
		close();
	}
}

void MainWindow::stopVideo () {
    ui->label1->clear();
    ui->label2->clear();
	cam_src_ptr->set_term_sig (1);
	//udp_src_ptr->set_term_sig (1);
}

/*
void MainWindow::updateFrameAfter (cv::Mat m) {
	std::chrono::steady_clock::time_point Tbegin, Tend;
	Tbegin = std::chrono::steady_clock::now ();

	if (!m.empty()) {
		cv::cvtColor(m, m, cv::COLOR_BGR2RGB); 
		QImage frame (m.data, m.cols, m.rows, m.step, QImage::Format_RGB888);

		annotate_frame (frame, Tudpsrc_prev_end, Tbegin, udpsrc_Fcnt, udpsrc_FPS);

		ui->label2->setPixmap(QPixmap::fromImage(frame));

		Tudpsrc_prev_end = std::chrono::steady_clock::now ();
		after_count++;
	}
	else printf("EMPTY\n");
}
*/

void MainWindow::updateFrameBefore () {
	GstSample* sample = nullptr;
	cv::Mat RGB;
	std::unique_lock<std::mutex> lock (cam_src_ptr->cb_dat.lock_q);
	{
		if (!cam_src_ptr->cb_dat.frame_q.empty()){
			sample = cam_src_ptr->cb_dat.frame_q.front();
			cam_src_ptr->cb_dat.frame_q.pop();
		}
		else {
			printf ("QUEUE EMPTY\n");
			return;
		}
	}
	std::chrono::steady_clock::time_point Tbegin;
	Tbegin = std::chrono::steady_clock::now ();

	if (sample) {
		GstBuffer* buffer = gst_sample_get_buffer (sample);
		// Extract timestamp from payload / latency calculation

		GstMapInfo map;
		if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
		    gst_buffer_unmap (buffer, &map);
		    gst_sample_unref (sample);
		    return;
		}

		// Extract Image data and caps
		GstCaps* caps = gst_sample_get_caps (sample);
		GstStructure* caps_struct = gst_caps_get_structure (caps, 0);
		char* caps_s = gst_caps_to_string (caps);
		int width, height;
		gst_structure_get_int (caps_struct, "width", &width);
		gst_structure_get_int (caps_struct, "height", &height);

		// Conversion I420 to BGR
		size_t y_size = width * height;
		size_t u_size = y_size / 4;
		size_t v_size = u_size; 

		cv::Mat I420 (height + height / 2, width, CV_8UC1, map.data);
		cv::cvtColor(I420, RGB, cv::COLOR_YUV2RGB_I420); 
		//cv::imwrite ("test.jpg", BGR);
		
		gst_buffer_unmap (buffer, &map);
		gst_sample_unref (sample);

		QImage frame (RGB.data, RGB.cols, RGB.rows, RGB.step, QImage::Format_RGB888);
		//annotate_frame (frame, Tcamsrc_prev_end, Tbegin, camsrc_Fcnt, camsrc_FPS);
		ui->label1->setPixmap(QPixmap::fromImage(frame));

		// Calculate FPS and send to plotter
	/*
		std::chrono::steady_clock::time_point Tnow = std::chrono::steady_clock::now ();
		float run_time = std::chrono::duration_cast <std::chrono::milliseconds> (Tnow-TP_camsrc_start).count ();
		frame_count++;
		fps.insert (fps.begin(), (float)frame_count / run_time);
	*/
		//before_count++;
		frame_count++;
	}
	else printf("EMPTY\n");
	return;
}

void MainWindow::toggleVideo () {
	cam_src_ptr->set_toggle_sig (1);
	//udp_src_ptr->set_toggle_sig (1);
}

void MainWindow::createAxis(int num_updates) {
	int length_x = 1670;
	int length_y = -150;
	int axis_label_space = 90;
	int tick_space = 27;
	int tick_len = 5;
	char label_buf[8];
	int c = 0;
	int e = 0;
	int x = 0;
	// Create and add x-axis
	QGraphicsLineItem *xAxis = new QGraphicsLineItem(0, 0, length_x, 0);
	scene->addItem(xAxis);

	// Create and add y-axis-1 (bitrate)
	QGraphicsLineItem *yAxis_1 = new QGraphicsLineItem(0, length_y, 0, 0);
	scene->addItem(yAxis_1);
	
	// Create and add y-axis-2 (fps)
	QGraphicsLineItem *yAxis_2 = new QGraphicsLineItem(length_x, length_y, length_x, 0);
	scene->addItem(yAxis_2);

	// Add axis labels
	QGraphicsTextItem *xLabel = new QGraphicsTextItem("Running Time (sec)");
	xLabel->setPos(length_x/2, 30);
	scene->addItem(xLabel);
	xLabel->setTransformOriginPoint(xLabel->boundingRect().center());
	xLabel->setScale(1.7);

	QGraphicsTextItem *yLabel_1 = new QGraphicsTextItem("Bitrate (Mb/s)");
	yLabel_1->setDefaultTextColor(Qt::red);
	yLabel_1->setPos(-100, length_y/2);
	scene->addItem(yLabel_1);
	yLabel_1->setRotation(270);
	yLabel_1->setTransformOriginPoint(yLabel_1->boundingRect().center());
	yLabel_1->setScale(1.7);

	QGraphicsTextItem *yLabel_2 = new QGraphicsTextItem("FPS");
	yLabel_2->setDefaultTextColor(Qt::blue);
	yLabel_2->setPos(length_x+30, length_y/2);
	scene->addItem(yLabel_2);
	yLabel_2->setRotation(270);
	yLabel_2->setTransformOriginPoint(yLabel_2->boundingRect().center());
	yLabel_2->setScale(1.7);

	// Draw tick marks and labels on the x-axis
	c = num_updates; //leave c equal to b
	for (x = 0; (x <= length_x + 1); x += tick_space) {
		QGraphicsLineItem *xTick = new QGraphicsLineItem(x, 0 - tick_len, x, tick_len);
		scene->addItem(xTick);
		if (c % 2 == 0)	{
			snprintf (label_buf, 8, "%d", c);
			QGraphicsTextItem *xTickLabel = new QGraphicsTextItem(label_buf);
			xTickLabel->setPos(x-7, 10);
			scene->addItem (xTickLabel);
			xTickLabel->setTransformOriginPoint(xTickLabel->boundingRect().center());
			xTickLabel->setScale(1.6);
		}
		c -= 1;
		if (c < 0) break;
	}

	// Draw ticks and labels on the y-axis 1
	for (x = 0; x >= length_y; x -= 30) {
		QGraphicsLineItem *yTick_1 = new QGraphicsLineItem(0 - tick_len, x, tick_len, x);
		scene->addItem(yTick_1);
		snprintf (label_buf, 8, "%d", e);
		QGraphicsTextItem *yTickLabel_1 = new QGraphicsTextItem(label_buf);
		yTickLabel_1->setPos(-35, x-8);
		scene->addItem(yTickLabel_1);
		yTickLabel_1->setTransformOriginPoint(yTickLabel_1->boundingRect().center());
		yTickLabel_1->setScale(1.6);
		e+=4;
	}

	e = 0;
	// Draw ticks and labels on the y-axis 1
	for (x = 0; x >= length_y; x -= 30) {
		QGraphicsLineItem *yTick_2 = new QGraphicsLineItem(length_x - tick_len, x, length_x + tick_len, x);
		scene->addItem(yTick_2);
		snprintf (label_buf, 8, "%d", e);
		QGraphicsTextItem *yTickLabel_2 = new QGraphicsTextItem(label_buf);
		yTickLabel_2->setPos(length_x + tick_len, x-8);
		scene->addItem(yTickLabel_2);
		yTickLabel_2->setTransformOriginPoint(yTickLabel_2->boundingRect().center());
		yTickLabel_2->setScale(1.6);
		e+=24;
	}
}

long int get_num_data (std::vector <std::vector <double>>* data_ptr) {
	std::vector<std::vector <double>>::iterator it;
	long int size = 0;
	it = data_ptr->end ();
	it--;
	long int ts_start = (long int)(*it)[0];
	it = data_ptr->begin ();
	long int ts_cur = (long int)(*it)[0];
	printf ("RUNNING TIME: %ld | ", ts_cur - ts_start);
	for (it = data_ptr->begin (); it != data_ptr->end(); it++) {
		size += (*it)[1];
	}
	return size;
}

void MainWindow::updateGraph() {
    // Update Metrics
    int update_interval = 1000; // 1sec
    float br = 0;
    int frame_cnt = frame_count;
    if (datab_ptr->size () > 0) {
        br = (float)cam_src_ptr->probe_dat.num_recv_bits / 1000000.0; // Mbps
        br_running += br;
        brs.insert(brs.begin (), br);
        cam_src_ptr->probe_dat.num_recv_bits = 0;
    }
    else return;
    fps.insert (fps.begin(), frame_cnt);
    fps_running += frame_cnt;
    frame_count = 0;
    num_updates++;

    // Update Averages
    avg_br.insert (avg_br.begin (), br_running / num_updates);
    avg_fps.insert (avg_fps.begin (), fps_running / num_updates);

    // Plot points on dynamic graph
	QPainterPath p_br;
	QPainterPath p_br_avg;
	QPainterPath p_fps;
	QPainterPath p_fps_avg;
	std::vector <double>::iterator it_br;
	std::vector <int>::iterator it_fps;
	int x_offset = 27;
    double x = 0, y = 0;
    int i;
    
	p_br.moveTo(0, 0);
	p_br_avg.moveTo(0, 0);
	p_fps.moveTo(0, 0);
	p_fps_avg.moveTo(0, 0);
    if (num_updates <= maxDataPoints) {
        for (i = 0; i < num_updates; i++) {
			x = i * x_offset;

			y = brs[i] * -150.0 / 20.0;
			p_br.lineTo(x, y);
			y = avg_br[i] * -150.0 / 20.0;
			p_br_avg.lineTo(x, y);

			y = fps[i] * -150.0 / 120.0;
			p_fps.lineTo(x, y);
			y = avg_fps[i] * -150.0 / 120.0;
			p_fps_avg.lineTo(x, y);
        }
    }
    else {
        int i_offset = num_updates - maxDataPoints;
        for (i = 0; i < maxDataPoints; i++) {
			x = i * x_offset;

			y = brs[i] * -150.0 / 20.0;
			p_br.lineTo(x, y);
			y = avg_br[i] * -150.0 / 20.0;
			p_br_avg.lineTo(x, y);

			y = fps[i] * -150.0 / 120.0;
			p_fps.lineTo(x, y);
			y = avg_fps[i] * -150.0 / 120.0;
			p_fps_avg.lineTo(x, y);
        }
    }

	// Clear and udpate the scene
	scene->clear();
	scene->addPath(p_br, red);
	scene->addPath(p_br_avg, dark_red);
	scene->addPath(p_fps, blue);
	scene->addPath(p_fps_avg, dark_blue);
	createAxis(num_updates - 1);
}



