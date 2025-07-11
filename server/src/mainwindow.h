#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QApplication>
#include <QMainWindow>
#include <QTimer>
#include <QKeyEvent>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QGraphicsLineItem>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QTimer>
#include <QPainterPath>
#include <QPen>
#include <QFont>
#include <QDateTime>

#include <opencv2/core.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>

#include <stdio.h>
#include <fstream>
#include <string>
#include <chrono>
#include <unistd.h>
#include <thread>
#include <vector>

#include "vid_proc.h"

Q_DECLARE_METATYPE(cv::Mat)

struct Config {
	int res[2];
	int fps;
	char dev[16];
	char format[16];
	char server_ip [16];
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, char* cam_src_str = nullptr, std::vector<std::vector<double>> *dataAFTER=NULL);
    ~MainWindow ();
protected:
	void keyPressEvent (QKeyEvent *event) override;
signals:
	void stop_vid_proc (int var);
private slots:
    void toggleVideo ();
    void stopVideo ();
    void updateFrameBefore ();
    //void updateFrameAfter (cv::Mat frame);
private:
	Ui::MainWindow *ui;
	Video_Proc* cam_src_ptr;

	// Framerate Tracking
	float camsrc_FPS [16];
	int camsrc_Fcnt;
	std::chrono::steady_clock::time_point TP_camsrc_start;

	void setupUI ();
	virtual void closeEvent (QCloseEvent *event) override;

	// Dynamic graphing with graphics view
	std::vector <double> brs;
	std::vector <double> avg_br;
	std::vector <int> fps;
	std::vector <int> avg_fps;
    float br_running = 0;
    int fps_running = 0;
	int frame_count = 0;
	long int running_total = 0;
	QGraphicsScene *scene;
	QPainterPath path;
	QPen red = QPen(Qt::red);
	QPen dark_red = QPen(Qt::darkRed);
	QPen blue = QPen(Qt::blue);
	QPen dark_blue = QPen(Qt::darkBlue);
	QTimer *timer;
	std::chrono::steady_clock::time_point start_time;
	std::chrono::steady_clock::time_point end_time;
	QGraphicsView RealTimeGraph;
	int num_updates = 0;
	int maxDataPoints = 62;
	double tot_lat = 0;
	void createAxis (int b);
	void updateGraph ();

	// Packet size and timestamp collection
	std::vector<std::vector <double>> *datab_ptr;
	long int OGTimeStamp;
	double runtotal = 0;
};

#endif // MAINWINDOW_H
