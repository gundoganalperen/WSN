#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "qextserialport.h"         // Enables use of the qextserialport library.
#include "qextserialenumerator.h"   // Helps list of open ports.
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QMdiArea>
#include <QMap>

///CUSTOM DEFINED STRUCTURES
struct Node{
    int id;         //node id
    int x;          //coordinate
    int y;          //coordinate
    int edge_dest;  //id of the next node (to draw the line)
    int edge_cost;  // cost of the line = Total Hop
    int seq_no;     // sequence number
    bool draw;      //defines if the node will be drawn
};
struct Segment{
    int id;         //id of segment
    int x1;         //X-coordinate of node_1
    int x2;         //X-coordinate of node_2
    int y;          //Y-coordinate
    int node_id_1;  //id of node 1
    int node_id_2;  //id of node 1
    int count;      //current count of cars in the segment  TODO delete this !!!
    int zero_point; //used to rebalance the segment count   TODO delete this !!!
    QColor color;   //color of the segments
    bool draw;      //if the segment should be drawn
    bool flash;     //if count number should flash
};
struct DataLogger{
    int id;         //ID of RSU sending data
    float temp;     //temperature
    float batt;     //voltage of battery
    int timer_count;      //keeps the ticks for data validity timer
    int enabled;    //shows validity of data
    int count;      //stores object detection counter
    int zero_point; //used to rebalance the object detection count
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    virtual void paintEvent(QPaintEvent *event);
    void segments_init();   //initialise segments structure
    void RSU_table_init();      //initialise RSU table
    void datalogger_init(); //initialise datalogger table
    void MU_mapping_init(); //initialise MU message id to string mapping table

    void datalogger_update();   //convert from datalogger table to UI element values
    void serial_send(QString);  //send serial string command to open port
    QString int_to_qstring(int);    //convert int to QString
    void segments_counter(int, int);    //Decrement and incement segment with search by id function
    void sector_handler();              //Handling the update of sectors in the GUI
    void sector_count_handler();
private slots:
    ///SERIAL FUNCTIONS
    void receive();                             // Serial receive data handling
    void on_serial_search_button_clicked();     // Searches for serial ports
    void on_serial_send_button_clicked();       // Sends string command to serial port
    void on_serial_open_button_clicked();       // Opens the serial port
    void on_serial_close_button_clicked();      // Closes the serial port
    void on_serial_clear_output_button_clicked();   // Clears the output text edit
    void on_serial_command_tedit_blockCountChanged(int newBlockCount);  //Sends serial command upon pressing enter
    ///TIMER EXPIRING FUNCTIONS
    void datalogger_timer_expire();             //check validity of datalogger data
    void segment_flash_timer_expire();          //function excecuted when  timer expirese, used for unicast flashing
    void set_selfID();                          // checks the routing table and defines the ID of the current node
    void collision_timer_expire();              //timer to flash warning message
    void on_serial_MUsend_button_clicked();     //Sends MU request via serial
    ///BUTTON FUNCTIONS
    void on_reorder_button_clicked();           //initiates the reordering of segments
    void on_count_reset_button_clicked();       //resets the segments counter to 0
    void on_clear_warning_button_clicked();     //clears the warning and clears collision
    void on_power_button_clicked();             //sends command to change TX power
    void on_power_cb_currentIndexChanged(int index);    //indicates when different TX power is selected
    ///DEBUG FUNCTIONS
    void on_test_button_clicked();
    void on_test_button_2_clicked();
    void on_test_button_3_clicked();


private:
    Ui::MainWindow *ui;
    QextSerialPort port;
    QMessageBox error;
};
#endif // MAINWINDOW_H
