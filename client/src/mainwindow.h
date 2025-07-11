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

#include <opencv2/core/core.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <unistd.h>
#include <thread>
#include <vector>
#include <chrono>

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
    MainWindow(QWidget *parent = nullptr, char* udp_src_str = nullptr, char* cam_src_str = nullptr, std::vector<std::vector<double>> *dataBEFORE=NULL, std::vector<std::vector<double>> *dataAFTER=NULL);
    ~MainWindow ();
protected:
	void keyPressEvent (QKeyEvent *event) override;
signals:
	void stop_vid_proc (int var);
private slots:
    void toggleVideo ();
    void stopVideo ();
    void updateFrameBefore (cv::Mat frame);
    void updateFrameAfter (cv::Mat frame);
private:
    void setupUI ();
	void createAxis (int b);
	void updateGraph ();
	virtual void closeEvent (QCloseEvent *event) override;

    Ui::MainWindow *ui;
	Video_Proc* cam_src_ptr;
	Video_Proc* udp_src_ptr;

	// FPS measurement
	QPainter label2_painter;
	float udpsrc_FPS [16];
	int udpsrc_Fcnt;
	std::chrono::steady_clock::time_point Tudpsrc_prev_end;
	float camsrc_FPS [16];
	int camsrc_Fcnt;
	std::chrono::steady_clock::time_point Tcamsrc_prev_end;

	// Dynamic graphing with graphics view
    std::vector <double> bitrates;
    std::vector <double> avg_bitrates;
    QGraphicsScene *scene;
    QPainterPath path;
    QPen pen;
    QPen pen2;
    QTimer *timer;
    //int dataPointCount = 0;
    QGraphicsView RealTimeGraph;
    int num_updates = 0;
    int maxDataPoints = 75;
    double tot_lat = 0;

	// Packet size and timestamp collection
    std::vector<std::vector <double>> *datab_ptr;
    std::vector<std::vector <double>> *dataa_ptr;
    long int OGTimeStamp;
    double runtotal = 0;
};

#endif // MAINWINDOW_H
