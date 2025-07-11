#include "mainwindow.h"
#include "ui_mainwindow.h"

// Globals set by main, needed for gst -> opencv conversion and display
extern const int WIDTH;
extern const int HEIGHT;

int before_count = 0;
int after_count = 0;
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
MainWindow::MainWindow (QWidget* parent, char* udp_src_str, char* cam_src_str, std::vector<std::vector<double>> *dataBEFORE, std::vector<std::vector<double>> *dataAFTER  ) : QMainWindow(parent), ui(new Ui::MainWindow) {
	// Set up UI
    //ui->setupUi(this);
    //setupUI();
	
	//udp_src_ptr = new Video_Proc (this, udp_src_str, 1, NULL, dataAFTER);
	cam_src_ptr = new Video_Proc (this, cam_src_str, 0, NULL, dataBEFORE);
	cam_src_ptr->start ();

	// Connect signals for consumer pipeline
	/*
	connect (udp_src_ptr, &Video_Proc::finished, udp_src_ptr, &Video_Proc::deleteLater);
	connect (udp_src_ptr, &Video_Proc::finished, udp_src_ptr, &Video_Proc::quit);
	connect (udp_src_ptr, &Video_Proc::new_frame_sig, this, &MainWindow::updateFrameAfter);
	connect (this, &MainWindow::stop_vid_proc, udp_src_ptr, &Video_Proc::set_term_sig);	

	// Set up for FPS measurements
    for (int i=0; i<16; i++) udpsrc_FPS[i] = 0.0;
	udpsrc_Fcnt = 0;
	Tudpsrc_prev_end = std::chrono::steady_clock::now ();
	udp_src_ptr->start ();
	
	// Connect signals for producer pipeline
	connect (cam_src_ptr, &Video_Proc::finished, cam_src_ptr, &Video_Proc::deleteLater);
	connect (cam_src_ptr, &Video_Proc::finished, cam_src_ptr, &Video_Proc::quit);
	connect (cam_src_ptr, &Video_Proc::new_frame_sig, this, &MainWindow::updateFrameBefore);
	connect (this, &MainWindow::stop_vid_proc, cam_src_ptr, &Video_Proc::set_term_sig);

	// Set up for FPS measurements
    for (int i=0; i<16; i++) camsrc_FPS[i] = 0.0;
	camsrc_Fcnt = 0;
	Tcamsrc_prev_end = std::chrono::steady_clock::now ();
	cam_src_ptr->start ();

	// To manipulate pipelines
	connect (ui->startButton, &QPushButton::clicked, this, &MainWindow::toggleVideo);
	connect (ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopVideo);

    scene = new QGraphicsScene(this);
    ui->graph->setScene(scene);

    // Initialize path and pen for the graph line
    // Set up initial graph properties
    this->dataa_ptr=dataAFTER;
    this->datab_ptr=dataBEFORE;

    pen = QPen(Qt::blue);
    pen.setWidth(2);
    pen2 = QPen(Qt::red);
    pen2.setWidth(2);

    // Create axes
    createAxis(0);

    // Start the timer to update the graph
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateGraph);
    timer->start(1000); // Update every 100 milliseconds
	*/
}

// Free resources
MainWindow::~MainWindow () {
	printf("DESTROY MAINWINDOW\n");
	delete cam_src_ptr;
	delete udp_src_ptr;
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
	if (cam_src_ptr->pipe_running || udp_src_ptr->pipe_running) {
		QMessageBox popup;
		popup.setWindowTitle ("STOP VIDEO!");
		popup.setText ("Please press stop before closing the main window.");
		popup.exec ();
		event->ignore ();
	}
	else {
		cam_src_ptr->wait ();
		udp_src_ptr->wait ();
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
	std::vector <double>::iterator it2;
    ui->label1->clear();
    ui->label2->clear();
	cam_src_ptr->set_term_sig (1);
	udp_src_ptr->set_term_sig (1);

	if (timer->isActive ()) timer->stop();

	std::chrono::steady_clock::time_point Tnow;
	Tnow = std::chrono::steady_clock::now ();
	auto now = Tnow.time_since_epoch ();
	int time = std::chrono::duration_cast <std::chrono::seconds> (now).count();
	char filename[128];
	char path[64];
	strcpy (path, "/home/rock/DCV_NETPERF/client/output_files");
	snprintf (filename, 128, 
			"%s/output_%d.txt", path, time);
	std::ofstream outfile (filename, std::ofstream::out);

	if (!outfile.is_open()) {
		std::cerr << "Error opening file!";
	}
	for (it2 = bitrates.end()--; it2 >= bitrates.begin(); it2--) {
		outfile << it2[0] << std::endl;
	}
	outfile.close();
}

void MainWindow::updateFrameAfter (cv::Mat m) {
	std::chrono::steady_clock::time_point Tbegin, Tend;
	Tbegin = std::chrono::steady_clock::now ();

	if (!m.empty()) {
		cv::cvtColor(m, m, cv::COLOR_BGR2RGB); 
		QImage frame (m.data, m.cols, m.rows, m.step, QImage::Format_RGB888);
		if (frame.isNull ()) printf ("NULL\n");

		annotate_frame (frame, Tudpsrc_prev_end, Tbegin, udpsrc_Fcnt, udpsrc_FPS);

		ui->label2->setPixmap(QPixmap::fromImage(frame));

		Tudpsrc_prev_end = std::chrono::steady_clock::now ();
		after_count++;
	}
	else printf("EMPTY\n");
}

void MainWindow::updateFrameBefore (cv::Mat m) {
	std::chrono::steady_clock::time_point Tbegin;
	Tbegin = std::chrono::steady_clock::now ();

	if (!m.empty()) {
		cv::cvtColor(m, m, cv::COLOR_BGR2RGB); 
		QImage frame (m.data, m.cols, m.rows, m.step, QImage::Format_RGB888);
		if (frame.isNull ()) printf ("NULL\n");

		annotate_frame (frame, Tcamsrc_prev_end, Tbegin, camsrc_Fcnt, camsrc_FPS);

		ui->label1->setPixmap(QPixmap::fromImage(frame));

		Tcamsrc_prev_end = std::chrono::steady_clock::now ();
		before_count++;
	}
	else printf("EMPTY\n");
}

void MainWindow::toggleVideo () {
	cam_src_ptr->set_toggle_sig (1);
	udp_src_ptr->set_toggle_sig (1);
	if (timer->isActive()) {
		timer->stop();
	}
	else {
		timer->start(1000);
	}
}

void MainWindow::createAxis(int num_updates) {
	int length_x = 1775;
	int length_y = -150;
	int tick_space = 24;
	int tick_len = 5;
	char label_buf[8];
	int c = 0;
	int e = 0;
	int x = 0;
	// Create and add x-axis
	QGraphicsLineItem *xAxis = new QGraphicsLineItem(0, 0, length_x, 0);
	scene->addItem(xAxis);

	// Create and add y-axis
	QGraphicsLineItem *yAxis = new QGraphicsLineItem(0, length_y, 0, 0);
	scene->addItem(yAxis);

	// Add axis labels
	QGraphicsTextItem *xLabel = new QGraphicsTextItem("Time(sec)");
	xLabel->setPos(900, 30);
	scene->addItem(xLabel);
	xLabel->setTransformOriginPoint(xLabel->boundingRect().center());
	xLabel->setScale(1.7);

	QGraphicsTextItem *yLabel = new QGraphicsTextItem("BitRate(Mb/s)");
	yLabel->setPos(-100, -100);
	scene->addItem(yLabel);
	yLabel->setRotation(270);
	yLabel->setTransformOriginPoint(yLabel->boundingRect().center());
	yLabel->setScale(1.7);

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

	// Draw ticks and labels on the y-axis
	for (x = 0; x >= length_y; x -= 30) {
		QGraphicsLineItem *yTick = new QGraphicsLineItem(0 - tick_len, x, tick_len, x);
		scene->addItem(yTick);
		snprintf ( label_buf, 8, "%d", e);
		QGraphicsTextItem *yTickLabel = new QGraphicsTextItem(label_buf);
		yTickLabel->setPos(-35, x-8);
		scene->addItem(yTickLabel);
		yTickLabel->setTransformOriginPoint(yTickLabel->boundingRect().center());
		yTickLabel->setScale(1.6);
		e+=4;
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
	// Draw the graph
	QPainterPath path;
	QPainterPath path2;
	//printf("Size A/B %ld / %ld\n", dataa_ptr->size(), datab_ptr->size());
    std::vector<std::vector <double>>::iterator it;
	std::vector <double>::iterator it2;
	double bitrate = 0;
	double avg_bitrate = 0;	
	int i = 0;

	if ((dataa_ptr->size () > 0)) {
		/*
		if (num_updates == 0) {
			it=datab_ptr->end();
			it--;
			OGTimeStamp = (long int)(*it)[0];
			printf("INFO: Stream begin at %ld.\n", OGTimeStamp);
		}
		*/
		udp_src_ptr->probe_dat.data_lock.lock ();
		//printf ("Size A: %0.3f Mb\n", (float)(get_num_data (dataa_ptr))/1000000);
		bitrate = find_bitrate (dataa_ptr, OGTimeStamp, num_updates);
		udp_src_ptr->probe_dat.data_lock.unlock ();
		bitrates.insert (bitrates.begin (), bitrate / 1000000);
	}
	else {
		printf ("NO DATA\n");
		bitrates.insert (bitrates.begin (), 0);
	}

	// Calculate average bitrate
	if (bitrates.size () <= 10) {
		it2 = bitrates.begin ();
		for (i = 0; i < (int)bitrates.size (); i++) {
			avg_bitrate += it2[0];
			it2++;	
		}
		avg_bitrate = avg_bitrate / i;
		avg_bitrates.insert (avg_bitrates.begin (), avg_bitrate);	
	}
	else {
		it2 = bitrates.begin();
		for (i = 0; i < 10; i++) {
			avg_bitrate += (it2[0]);
			it2++;	
		}
		avg_bitrate = avg_bitrate / 10;
		avg_bitrates.insert (avg_bitrates.begin (), avg_bitrate);
	}
	// Remove last element to maintain 75 total
	if (avg_bitrates.size () > 75) avg_bitrates.pop_back ();
	 
	// Draw instantanious bitrate on graph
	path.moveTo(0, 0);
	if (bitrates.size () < maxDataPoints) {
		for (i = 0; i < (int)bitrates.size(); i++) {
			double x = i * (width()-120) / maxDataPoints;
			double y = bitrates[i] * -150 / 20;
			path.lineTo(x, y);
		}
	}
	else {
		for (i = 0; i < maxDataPoints; i++) {
			double x = i * (width()-120) / maxDataPoints;
			double y = bitrates[i] * -150 / 20;
			path.lineTo(x, y);
		}
	}	

	// Draw average bitrate on graph
	path2.moveTo(0, 0);
	for (i = 0; i < (int)avg_bitrates.size(); i++) {
		double x = i * (width() - 120) / maxDataPoints;
		double y = avg_bitrates[i] * -150 / 20;
		path2.lineTo(x, y);
	}
	
	// Clear the scene
	scene->clear();
	scene->addPath(path, pen);
	scene->addPath(path2, pen2);
	createAxis(num_updates);
	num_updates += 1;
}
