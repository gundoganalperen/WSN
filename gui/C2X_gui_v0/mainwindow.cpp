#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qdebug.h>
#include <QVector>
#include <QSize>
#include <QTimer>

#define n 8 //max number of RSU supported
DataLogger datalogger[2*n]; // structure for keeping temp and batt of RSU
Segment seg_table[2*n]; // structure to keep segment (road between RSU) information
Node RSU_table[2*n]; // structure to keep RSU information
QTimer *segment_flash_timer, *selfID_timer, *collision_timer, *datalogger_timer, *MU_response_timer;
Node collision_MU_node;
QMap<int, QString> MU_mapping;
bool routes_drawing, segments_drawing,collision_drawing,segments_ordering;
int cnt_segment, first_segment_node, last_segment_node, selfID = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ////TIMER SETUP
    datalogger_timer = new QTimer(this);
    segment_flash_timer = new QTimer(this);
    selfID_timer = new QTimer(this);
    collision_timer = new QTimer(this);
    MU_response_timer = new QTimer(this);
    connect(datalogger_timer, SIGNAL(timeout()), this, SLOT(datalogger_timer_expire()));
    connect(segment_flash_timer, SIGNAL(timeout()), this, SLOT(segment_flash_timer_expire()));
    connect(selfID_timer, SIGNAL(timeout()), this, SLOT(set_selfID()));
    connect(collision_timer, SIGNAL(timeout()), this, SLOT(collision_timer_expire()));
    connect(MU_response_timer, SIGNAL(timeout()), this, SLOT(on_serial_MUsend_button_clicked()));

    ////INITIALISATION
    RSU_table_init();
    segments_init();
    datalogger_init();
    MU_mapping_init();
    on_serial_search_button_clicked();
    //on_serial_open_button_clicked();
}
MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::paintEvent(QPaintEvent *event){
    QPainter painter(this);
    //Defining Colors
    QColor RSU_fill_color = QColor(190,190,190);
    QColor single_hop_color = QColor(0,200,100);
    QColor multi_hop_color = QColor(250,50,50);

    QBrush brush (RSU_fill_color, Qt::SolidPattern);
    QPen pen;
    QFont font;
    font.setBold(true);
    font.setWeight(75);
    font.setPixelSize(25);
    pen.setWidth(1);
    pen.setColor(Qt::yellow);
    painter.setPen(pen);
    painter.setBrush(brush);
    painter.setFont(font);
    if (routes_drawing){    //goes through the router table struct
        for (int i=n-1 ; i>=0 ; i--){
            QLine edge; // line to hold the edge to be drawn
            QPoint RSU_point = QPoint(RSU_table[i].x,RSU_table[i].y); // the point of the RSU
            QRect RSU = QRect(RSU_point,QSize(50,50)); // RSU object
            //TODO - draw a text for the total hop count
            //checks whether a entry on the table shall be drawn
            if(RSU_table[i].draw){
                //draws the edges between RSU
                if (RSU_table[i].edge_cost > 1){
                    edge.setP1(QPoint(RSU_table[i].x+25,RSU_table[i].y+25));
                    int dest_id = RSU_table[i].edge_dest - 1;
                    if (collision_MU_node.id == RSU_table[i].edge_dest) edge.setP2(QPoint(collision_MU_node.x+25,collision_MU_node.y+25));
//delete line       //if (collision_MU_node.edge_dest == collision_MU_node.id) edge.setP2(QPoint(RSU_table[0].x+25,RSU_table[0].y+25));
                    else edge.setP2(QPoint(RSU_table[dest_id].x+25,RSU_table[dest_id].y+25));
                    pen.setWidth(3);
                    pen.setColor(multi_hop_color);
                    painter.setPen(pen);
                    painter.setBrush(brush);
                    //QPoint text_point = edge.center();
                    //painter.drawEllipse(RSU);
                    //painter.drawLine(edge);
                    //painter.drawText(text_point, int_to_qstring(RSU_table[i].edge_cost));
                    //painter.drawText(RSU,Qt::AlignCenter, int_to_qstring(RSU_table[i].id));
                }
                if (RSU_table[i].edge_cost == 1){
                    edge.setP1(QPoint(RSU_table[i].x+25,RSU_table[i].y+25));
                    int dest_id = selfID - 1;
                    if (collision_MU_node.edge_dest == collision_MU_node.id) edge.setP2(QPoint(RSU_table[0].x+25,RSU_table[0].y+25));
                    else edge.setP2(QPoint(RSU_table[dest_id].x+25,RSU_table[dest_id].y+25));
                    pen.setWidth(3);
                    pen.setColor(single_hop_color);//green are the direct connections
                    painter.setPen(pen);
                    painter.setBrush(brush);
                    //QPoint text_point = edge.center();
                    //painter.drawEllipse(RSU);
                    //painter.drawLine(edge);
                    //painter.drawText(text_point, int_to_qstring(RSU_table[i].edge_cost));
                    //painter.drawText(RSU,Qt::AlignCenter, int_to_qstring(RSU_table[i].id));
                }
                if (RSU_table[i].edge_cost == 0){
                    //qDebug()<<"TotalHop == 0 @ " << i;
                    pen.setWidth(3);
                    pen.setColor(Qt::black);
                    painter.setPen(pen);
                    painter.setBrush(QBrush (Qt::white, Qt::SolidPattern));
                    //painter.drawEllipse(RSU);
                    //painter.drawText(RSU,Qt::AlignCenter, int_to_qstring(RSU_table[i].id));
                }
                //qDebug()<< "Node: " << i <<" dest: "<< RSU_table[i].edge_dest;
                if (RSU_table[i].edge_cost > 0 ){
                    painter.drawLine(edge);
                    //painter.drawText(edge.center(), int_to_qstring(RSU_table[i].edge_cost));
                }
                painter.drawEllipse(RSU);
                //painter.drawText(RSU,Qt::AlignCenter, int_to_qstring(RSU_table[i].id));
            }
        }
        for (int i=n-1 ; i>=0 ; i--){
            QLine edge; // line to hold the edge to be drawn
            QPoint RSU_point = QPoint(RSU_table[i].x,RSU_table[i].y); // the point of the RSU
            QRect RSU = QRect(RSU_point,QSize(50,50)); // RSU object
            //checks whether a entry on the table shall be drawn
            if(RSU_table[i].draw){
                //draws the edges between RSU
                if (RSU_table[i].edge_cost > 1){
                    edge.setP1(QPoint(RSU_table[i].x+25,RSU_table[i].y+25));
                    int dest_id = RSU_table[i].edge_dest - 1;
                    if (collision_MU_node.id == RSU_table[i].edge_dest) edge.setP2(QPoint(collision_MU_node.x+25,collision_MU_node.y+25));
//delete line       //if (collision_MU_node.edge_dest == collision_MU_node.id) edge.setP2(QPoint(RSU_table[0].x+25,RSU_table[0].y+25));
                    else edge.setP2(QPoint(RSU_table[dest_id].x+25,RSU_table[dest_id].y+25));
                    pen.setWidth(3);
                    pen.setColor(multi_hop_color);//red are the multihop paths
                    painter.setPen(pen);
                    painter.setBrush(brush);                }
                if (RSU_table[i].edge_cost == 1){
                    edge.setP1(QPoint(RSU_table[i].x+25,RSU_table[i].y+25));
                    int dest_id = selfID - 1;
                    if (collision_MU_node.edge_dest == collision_MU_node.id) edge.setP2(QPoint(RSU_table[0].x+25,RSU_table[0].y+25));
                    else edge.setP2(QPoint(RSU_table[dest_id].x+25,RSU_table[dest_id].y+25));
                    pen.setWidth(3);
                    pen.setColor(single_hop_color);//green are the direct connections
                    painter.setPen(pen);
                    painter.setBrush(brush);
                }
                if (RSU_table[i].edge_cost == 0){
                    pen.setWidth(3);
                    pen.setColor(Qt::black);
                    painter.setPen(pen);
                    painter.setBrush(QBrush (Qt::white, Qt::SolidPattern));
                }
                if (RSU_table[i].edge_cost > 0 ){
                    pen.setColor(Qt::black);
                    painter.setPen(pen);
                    painter.drawText(edge.center(), int_to_qstring(RSU_table[i].edge_cost));
                }
                painter.drawText(RSU,Qt::AlignCenter, int_to_qstring(RSU_table[i].id));
            }
        }
    }
    if (segments_drawing){
        //goes through the router table struct
        for (int i=0 ; i<n-1 ; i++){
            if(seg_table[i].draw){
                QPoint node_point_r = QPoint(seg_table[i].x1,seg_table[i].y);
                QPoint node_point_l = QPoint(seg_table[i].x2,seg_table[i].y);
                QRect node_r = QRect(node_point_r,QSize(40,40));
                QRect node_l = QRect(node_point_l,QSize(40,40));
                QLine seg = QLine(node_r.center(),node_l.center());
                node_l.center();
                pen.setColor(seg_table[i].color);
                pen.setWidth(3);
                brush.setColor(Qt::white);
                painter.setBrush(brush);
                painter.setPen(pen);
                painter.drawLine(seg);
                painter.drawRect(node_r);
                painter.drawRect(node_l);
                painter.drawText(node_r,Qt::AlignCenter, int_to_qstring(seg_table[i].node_id_1));
                painter.drawText(node_l,Qt::AlignCenter, int_to_qstring(seg_table[i].node_id_2));
                QPoint text_point = seg.center();
                text_point.setX(text_point.x()-5);
                text_point.setY(text_point.y()-15);

                if(seg_table[i].flash == true){
                    segment_flash_timer->start(250);
                    pen.setColor(Qt::yellow);
                    painter.setPen(pen);
                    painter.drawText(text_point, int_to_qstring(seg_table[i].count - seg_table[i].zero_point));
                }
                else{
                    painter.drawText(text_point, int_to_qstring(seg_table[i].count - seg_table[i].zero_point));
                }
            }
        }
    }
    if(collision_drawing){
        if (collision_MU_node.draw){
            QPoint MU_point = QPoint(collision_MU_node.x,collision_MU_node.y); // the point of the RSU
            QRect MU = QRect(MU_point,QSize(50,50)); // RSU object
            QLine edge;
            edge.setP1(QPoint(collision_MU_node.x+25,collision_MU_node.y+25));
            int dest_id = collision_MU_node.edge_dest - 1;
            if (collision_MU_node.edge_dest == collision_MU_node.id) edge.setP2(QPoint(RSU_table[0].x+25,RSU_table[0].y+25));
            else edge.setP2(QPoint(RSU_table[dest_id].x+25,RSU_table[dest_id].y+25));
            pen.setWidth(3);
            brush.setColor(QColor(255,255,155));
            pen.setColor(QColor(127,0,255));//red are the multihop paths
            painter.setPen(pen);
            painter.setBrush(brush);
            QPoint text_point = edge.center();
            painter.drawText(text_point, int_to_qstring(collision_MU_node.edge_cost));
            painter.drawLine(edge);
            painter.drawEllipse(MU);
            painter.drawText(MU,Qt::AlignCenter, int_to_qstring(collision_MU_node.id));
            painter.drawText(QPoint(collision_MU_node.x+15,collision_MU_node.y-10),"!!!");

        }
        QRect warning = QRect(QPoint(420,50), QSize(125,125));
        pen.setColor(Qt::red);
        brush.setColor(Qt::red);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.drawRect(warning);
        //draw the collsion node
        pen.setColor(Qt::black);
        painter.setPen(pen);
        font.setPixelSize(15);
        painter.setFont(font);
        QString collision_node  = "Collision MU: ";
        collision_node.append(int_to_qstring(collision_MU_node.id));
        painter.drawText(warning,Qt::AlignBottom,collision_node);
        //change to yellow to draw the !
        pen.setColor(Qt::yellow);
        painter.setPen(pen);
        font.setPixelSize(100);
        painter.setFont(font);
        painter.drawText(warning,Qt::AlignCenter,"!");
    }
}

////TIMER FUCNTIONS
void MainWindow::collision_timer_expire(){
    collision_timer->start(500);
    if(collision_drawing) collision_drawing = false;
    else collision_drawing = true;
    //qDebug()<<"Collision Timer Expired: "<<collision_drawing;
    QWidget::update();
}
void MainWindow::segment_flash_timer_expire()
{
    for (int i=0 ; i<n-1 ; i++){
        seg_table[i].flash = false;
    }
    segment_flash_timer->stop();
    QWidget::update();
}
void MainWindow::datalogger_timer_expire(){
    int lifetime = 30; //30 seconds lifetime for datalogger values
    for (int i=0 ; i<n-1 ; i++){
        if(datalogger[i].enabled)datalogger[i].timer_count++; //increment counter if slot is active
        if(datalogger[i].timer_count > lifetime){
            datalogger[i].timer_count = 0;
            datalogger[i].enabled = false; // set slot inactive and set counter to 0
        }
    }
    datalogger_update();
    datalogger_timer->start(1000);
    //qDebug()<<"Datalogger active";
}
void MainWindow::set_selfID(){
    for (int i=0 ; i<n ; i++){
        if(RSU_table[i].edge_dest == 0){
            selfID = i+1;
            qDebug()<< "SelfID: "<<selfID;
        }
    }
    //qDebug() << "SelfID=" << selfID;
    //check for self ID periodically to ensure false selfID being stuck
    selfID_timer->start(5000);
}
////SERIAL FUNCTIONS
void MainWindow::on_serial_search_button_clicked(){
    //change status label
    ui->status->setText("Searching for serial ports");

    // Get all available COM Ports and store them in a QList.
    QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();

    // Read each element on the list, but
    // add only USB ports to the combo box.
    ui->serial_interface_combo->clear();
    for (int i = 0; i < ports.size(); i++) {
        if (ports.at(i).portName.contains("USB")){
            ui->serial_interface_combo->addItem(ports.at(i).portName.toLocal8Bit().constData());
        }
    }
    // Show a hint if no USB ports were found.
    if (ui->serial_interface_combo->count() == 0){
        ui->status->setText("No USB ports available.\nConnect a USB device and try again.");
    }
    else {
        ui->status->setText("Search Completed!");
    }
}
void MainWindow::on_serial_open_button_clicked() {

    // The instance of qextserialport is configured like so:
    port.setQueryMode(QextSerialPort::EventDriven);
    port.setPortName("/dev/" + ui->serial_interface_combo->currentText());
    port.setBaudRate(BAUD115200);
    port.setFlowControl(FLOW_OFF);
    port.setParity(PAR_NONE);
    port.setDataBits(DATA_8);
    port.setStopBits(STOP_1);

    // Open sesame!!
    port.open(QIODevice::ReadWrite);

    // Check if the port opened without problems.
    if (!port.isOpen())
    {
        error.setText("Unable to open port!");
        error.show();
        return;
    }

    /* This is where the MAGIC HAPPENS and it basically means:
     *      If the object "port" uses its "readyRead" function,
     *      it will trigger MainWindow's "receive" function.
     */
    QObject::connect(&port, SIGNAL(readyRead()), this, SLOT(receive()));

    // Only ONE button can be enabled at any given moment.
    ui->serial_close_button->setEnabled(true);
    ui->serial_open_button->setEnabled(false);
    ui->serial_interface_combo->setEnabled(false);
    ui->serial_search_button->setEnabled(false);

    ui->serial_send_button->setEnabled(true);
    ui->serial_command_tedit->setEnabled(true);

    ui->power_button->setEnabled(true);
}
void MainWindow::on_serial_close_button_clicked()
{
    if (port.isOpen())port.close();
    ui->serial_close_button->setEnabled(false);
    ui->serial_open_button->setEnabled(true);
    ui->serial_interface_combo->setEnabled(true);

    ui->serial_send_button->setEnabled(false);
    ui->serial_command_tedit->setEnabled(false);
    ui->serial_search_button->setEnabled(true);

    ui->power_button->setEnabled(false);
}
void MainWindow::on_serial_clear_output_button_clicked(){
    ui->serial_output_tedit->clear();
    RSU_table_init();
    segments_init();
    datalogger_init();
    on_clear_warning_button_clicked();
    QWidget::update();
}
void MainWindow::on_serial_send_button_clicked()
{
    QString command;
    command = ui->serial_command_tedit->toPlainText();
    if (command.length() != 0){
        QByteArray byteArray = command.toLocal8Bit();

        byteArray.append('\n');
        port.write(byteArray);
        ui->status->setText("Command " + command +" send");
        ui->serial_command_tedit->clear();
    }
}
void MainWindow::on_serial_MUsend_button_clicked()
{
    QString command = "MU" + int_to_qstring(ui->MU_message_cb->currentIndex());
    serial_send(command);
    ui->MU_message_response_lb->setText("Waiting...");
    MU_response_timer->start(5000);

}
void MainWindow::on_serial_command_tedit_blockCountChanged(int newBlockCount)
{
    QString data = ui->serial_command_tedit->toPlainText();
    if(data.length() != 0){
        serial_send(data);
        data.remove(data.length()-1,data.length());
        ui->status->setText("Command " + data + " send");
        ui->serial_command_tedit->clear();
        qDebug()<<"TEditCommand char"<<"Command " + data + " send" <<"block count"<<newBlockCount;
    }
}
void MainWindow::serial_send(QString data) {
    QByteArray byteArray = data.toLocal8Bit();
    byteArray.append('\n');
    port.write(byteArray);
}
void MainWindow::receive()
{
    static QString str;
    char ch;
    while (port.getChar(&ch))
    {
        str.append(ch);
        if (ch == '\n')     // End of line, start decoding
        {
            qDebug() << "Received Line: " << str;
            str.remove("\n", Qt::CaseSensitive);
            str.remove("\r", Qt::CaseSensitive);
            ui->serial_output_tedit->append(str);
            ui->serial_output_tedit->ensureCursorVisible();
            if (str.contains("Type_0"))
            {
                QStringList list = str.split(QRegExp("\\s"));
                if(!list.isEmpty()){
                    //qDebug() << "List size " << list.size();
                    /*for (int i=0; i < list.size(); i++){
                        qDebug() << "List value "<< i <<" "<< list.at(i);
                    }
                    qDebug() << "Dest_Node: "<< list.at(3) << "Next_hop: "<< list.at(5) << "Total_hop: "<< list.at(7)
                        << "Metric: "<< list.at(9)<< "Seq_no: "<< list.at(11);*/

                    int dest_node = list.at(2).toInt();
                    int next_hop = list.at(4).toInt();
                    int tot_hop = list.at(6).toInt();
                    int metric = list.at(8).toInt();
                    int seq_no = list.at(10).toInt();
                    routes_drawing = true;
                    if (dest_node == collision_MU_node.id && collision_MU_node.draw){
                        collision_MU_node.edge_dest = next_hop;
                        collision_MU_node.edge_cost = tot_hop;
                        collision_MU_node.seq_no = seq_no;
                    }
                    else if (dest_node <=n){
                        RSU_table[dest_node-1].id = dest_node;
                        RSU_table[dest_node-1].edge_cost = tot_hop;
                        RSU_table[dest_node-1].edge_dest = next_hop;
                        RSU_table[dest_node-1].seq_no = seq_no;
                        if (seq_no%2 == 0 ) RSU_table[dest_node-1].draw = true;
                        else RSU_table[dest_node-1].draw = false;
                    }
                    ui->status->setText("Routing table updated!");
                }
                //ui->label_light->setText("Temp");
                //qDebug() << "Var value " << QString::number(value);
                //ui->lcdNumber_light->display(value);
                //ui->progressBar_light->setValue((int)value);
            }
            else if (str.contains("Type_6")){
                //segments_drawing = true;
                QStringList list = str.split(QRegExp("\\s"));
                if(!list.isEmpty()){
                    //\rType_6 source_adr %d prev_hop %d cnt %d OBJECT DETECTED!!!!\n"
                    int source_node = list.at(2).toInt();
                    int prev_hop = list.at(4).toInt();
                    int seg_cnt = list.at(6).toInt();
                    datalogger[source_node-2].count = seg_cnt;
                    sector_count_handler();
                    ui->status->setText("Segments counter updated!");
                    /*if (segments_ordering == true){
                        if(cnt_segment == 0){
                            first_segment_node = source_node;
                            seg_table[cnt_segment].node_id_1 = source_node;
                        }
                        else if(source_node == first_segment_node && cnt_segment > 1){
                            segments_ordering = false;
                            seg_table[cnt_segment-1].node_id_2 = source_node;
                            seg_table[cnt_segment-1].draw = true;
                            qDebug()<<"--Ordering Complete\n";
                        }
                        else{
                            seg_table[cnt_segment-1].node_id_2 = source_node;
                            seg_table[cnt_segment].node_id_1 = source_node;
                            last_segment_node = source_node;
                            seg_table[cnt_segment-1].draw = true;
                        }
                        cnt_segment++;
                    }
                    else{
                        segments_counter(source_node, seg_cnt);
                    }
                    for (int i=0 ; i<n-1 ; i++){
                        qDebug() <<"--SegID: "<<seg_table[i].id <<" NodeID1: "<<seg_table[i].node_id_1 <<" NodeID2: "<<seg_table[i].node_id_2 <<"Draw:"<<seg_table[i].draw << "count" << seg_table[i].count;
                    }
                    qDebug() << "seg_counter" << cnt_segment;*/

                }

            }
            else if (str.contains("Type_7")){
                collision_drawing = true;
                ui->clear_warning_button->setEnabled(true);
                ui->MU_message_cb->setEnabled(true);
                ui->serial_MUsend_button->setEnabled(true);
                QStringList list = str.split(QRegExp("\\s"));
                if(!list.isEmpty()){
                    int source_node = list.at(2).toInt();
                    int prev_hop = list.at(4).toInt();
                    collision_MU_node.id  =source_node;
                    collision_MU_node.edge_dest=prev_hop;
                    collision_MU_node.draw = true;
                    collision_timer->start(500);
                    qDebug() << "--COLLISION -SourceNode:"<<source_node<<"prev_hop"<<prev_hop;
                    ui->status->setText("Collision message received from id "+int_to_qstring(source_node)+"!");
                }
            }
            else if (str.contains("Type_8")){
                //Type_8 src_adr %d p_hop %d temp %d batt %d count %d Datalogger Report!
                QStringList list = str.split(QRegExp("\\s"));
                if(!list.isEmpty()){
                    int source_node = list.at(2).toInt();
                    int prev_hop = list.at(4).toInt();
                    float temp = list.at(6).toFloat();
                    float batt = list.at(8).toFloat();
                    int count = list.at(10).toInt();
                    datalogger[source_node-2].temp = temp / 100;
                    datalogger[source_node-2].batt = batt / 100;
                    datalogger[source_node-2].timer_count = 0;
                    datalogger[source_node-2].count = count;
                    datalogger[source_node-2].enabled = true;
                    sector_count_handler();
                    ui->status->setText("Data logger received from id "+int_to_qstring(source_node)+"!");
                }
            }
            else if (str.contains("Type_10")){
                //Type_10 MU_id %d MU_data %d
                QStringList list = str.split(QRegExp("\\s"));
                if(!list.isEmpty()){
                    int MU_id = list.at(2).toInt();
                    int data = list.at(4).toFloat();
                    ui->MU_message_response_lb->setText(MU_mapping[data]);
                    MU_response_timer->stop();
                    ui->status->setText("MU reply received from id "+int_to_qstring(MU_id)+"!");
                    //qDebug()<<"Mapping "<<MU_mapping[6];
                    //qDebug()<<"Mapping "<<MU_mapping[data];
                    //qDebug()<<"MU id"<<MU_id<<"MU_message "<<data;
                }
            }
            this->repaint();    // Update content of window immediately
            str.clear();
        }
        //check if selfID is set otherwise set it
        if (selfID == 0){
            set_selfID();
        }
        //update view
        QWidget::update();
    }
}
////SEGMENT FUNCTIONS
void MainWindow::on_count_reset_button_clicked()
{
    for (int i=0; i<n-1; i++){
        seg_table[i].zero_point = seg_table[i].count;
    }
    QWidget::update();
}
void MainWindow::on_reorder_button_clicked()
{
//    segments_init();
//    segments_ordering = true;
//    cnt_segment = 0;
    sector_handler();
    segments_drawing = true;
    QWidget::update();
}
////WARNING FUNCTIONS
void MainWindow::on_clear_warning_button_clicked()
{
    serial_send("MU 11");
    collision_timer->stop();
    collision_drawing=false;
    //ui->clear_warning_button->setEnabled(false);
    ui->MU_message_cb->setEnabled(false);
    ui->MU_message_response_lb->setText("...");
    ui->serial_MUsend_button->setEnabled(false);
    ui->status->setText("MU crash resolved.");
    QWidget::update();
}
////TX POWER FUNCTIONS
void MainWindow::on_power_button_clicked()
{
    int selected_power = ui->power_cb->currentText().split(" ").at(0).toInt();
    int selected_id = ui->power_cb->currentIndex();
    QString serial_command_array[14] = {"txp0","txp1","txp2","txp3","txp4","txp5","txp6","txp7","txp8","txp9","txp10","txp11","txp12","txp13"};
    serial_send(serial_command_array[selected_id]);
    QFont font = ui->power_button->font();
    font.setBold(false);
    ui->power_button->setFont(font);
    qDebug()<<"Selected:"<<selected_power<<"ID="<<selected_id;
}
void MainWindow::on_power_cb_currentIndexChanged(int index)
{
    QFont font = ui->power_button->font();
    font.setBold(true);
    ui->power_button->setFont(font);
}
////INITIALISATION FUNCTIONS
void MainWindow::segments_init(){
    segments_drawing = false;   //controlls if segments will be drawn on screen
    for (int i=0 ; i<n ; i++){
        seg_table[i].id = i;
        seg_table[i].node_id_1 = 0;
        seg_table[i].node_id_2 = 0;
        seg_table[i].count = 0;
        seg_table[i].x1 = 150+i*125;
        seg_table[i].x2 = 150+(i+1)*125;
        seg_table[i].y = 480;
        seg_table[i].color = Qt::black;
        seg_table[i].draw = false;
        seg_table[i].zero_point = 0;
    }
    cnt_segment = 0;
    first_segment_node = 0;
    last_segment_node = 0;
    //segments_ordering = true;   //controlls if segments are to be reordered
}
void MainWindow::RSU_table_init(){
    //RSU_table[pos] = {id,x,y,dest_id,tot_hop,seq_no, draw};
    routes_drawing = false;     //controlls if routes will be drawn on screen
    collision_drawing = false;  //controlls if the collisioned MU will be drawn on screen
    int offset = 0; //GUI parameter
    if (n<=8) {
        RSU_table[0] = {1, offset+60,     offset+60,     2,  0, 0, false};
        RSU_table[1] = {2, offset+150,    offset+25,     3,  0, 0, false};
        RSU_table[2] = {3, offset+240,    offset+60,     4,  0, 0, false};
        RSU_table[3] = {4, offset+270,    offset+140,    4,  0, 0, false};
        RSU_table[4] = {5, offset+240,    offset+240,    4,  0, 0, false};
        RSU_table[5] = {6, offset+150,    offset+270,    7,  0, 0, false};
        RSU_table[6] = {7, offset+60,     offset+245,    5,  0, 0, false};
        RSU_table[7] = {8, offset+25,     offset+140,    7,  0, 0, false};
    }
    else if (n>8){
        RSU_table[0] = {1, offset+  15,  offset+    174,     2,  0, 0, false};
        RSU_table[1] = {2, offset+  47,  offset+    76,     3,  0, 0, false};
        RSU_table[2] = {3, offset+  127, offset+    22,     4,  0, 0, false};
        RSU_table[3] = {4, offset+  220, offset+    22,    4,  0, 0, false};
        RSU_table[4] = {5, offset+  300, offset+    76,    4,  0, 0, false};
        RSU_table[5] = {6, offset+  335, offset+    174,    7,  0, 0, false};
        RSU_table[6] = {7, offset+  300, offset+    270,    5,  0, 0, false};
        RSU_table[7] = {8, offset+  220, offset+    325,    7,  0, 0, false};
        RSU_table[8] = {9, offset+  127, offset+    325,    7,  0, 0, false};
        RSU_table[9] = {10,offset+  47,  offset+    270,    7,  0, 0, false};
    }

    //initialise collision node
    collision_MU_node = {13, offset+150,     offset+140,    5,  0, 0, false};

}
void MainWindow::datalogger_init(){
    for (int i=0; i<n-1; i++){
        datalogger[i].enabled=false;
        datalogger[i].id=i+2;
        datalogger[i].temp=0.0;
        datalogger[i].batt=0.0;
        datalogger[i].timer_count = 0;
    }
    datalogger_timer->start(1000);
}
void MainWindow::MU_mapping_init(){
    //GTW 0 "Send location!" <-> MU 5 "Autobahn 9, km 23"
    //GTW 1 "Are you OK?" <-> 	MU 6 "No, we need help"
    //GTW 2 "How many people?" <-> MU 7 "3 people"
    //GTW 3 "Help is on way!" <-> MU 8 "Thank you."
    //GTW 4 "Is there fire?" <-> MU 9 "Yes, there is smoke"
    //GTW 5 "Firetruck on way!" <-> MU 10 "Hurry up!"
    MU_mapping[5]="Autobahn 9, km 23";
    MU_mapping[6]="No, we need help";
    MU_mapping[7]="3 people";
    MU_mapping[8]="Thank you";
    MU_mapping[9]="Yes, there is smoke";
    MU_mapping[10]="Hurry up!";
}
////UPDATE UI ELEMENT VALUES
void MainWindow::datalogger_update(){
    ui->     temp_pb_1->setValue(   datalogger[0].temp);
    ui->     batt_pb_1->setValue(   datalogger[0].batt);
    ui->     temp_pb_1->setEnabled( datalogger[0].enabled);
    ui->     batt_pb_1->setEnabled( datalogger[0].enabled);
    ui->temp_display_1->setEnabled( datalogger[0].enabled);
    ui->batt_display_1->setEnabled( datalogger[0].enabled);
    ui->temp_display_1->display(    datalogger[0].temp);
    ui->batt_display_1->display(    datalogger[0].batt);

    ui->     temp_pb_2->setValue(   datalogger[1].temp);
    ui->     batt_pb_2->setValue(   datalogger[1].batt);
    ui->     temp_pb_2->setEnabled( datalogger[1].enabled);
    ui->     batt_pb_2->setEnabled( datalogger[1].enabled);
    ui->temp_display_2->setEnabled( datalogger[1].enabled);
    ui->batt_display_2->setEnabled( datalogger[1].enabled);
    ui->temp_display_2->display(    datalogger[1].temp);
    ui->batt_display_2->display(    datalogger[1].batt);

    ui->     temp_pb_3->setValue(   datalogger[2].temp);
    ui->     batt_pb_3->setValue(   datalogger[2].batt);
    ui->     temp_pb_3->setEnabled( datalogger[2].enabled);
    ui->     batt_pb_3->setEnabled( datalogger[2].enabled);
    ui->temp_display_3->setEnabled( datalogger[2].enabled);
    ui->batt_display_3->setEnabled( datalogger[2].enabled);
    ui->temp_display_3->display(    datalogger[2].temp);
    ui->batt_display_3->display(    datalogger[2].batt);

    ui->     temp_pb_4->setValue(   datalogger[3].temp);
    ui->     batt_pb_4->setValue(   datalogger[3].batt);
    ui->     temp_pb_4->setEnabled( datalogger[3].enabled);
    ui->     batt_pb_4->setEnabled( datalogger[3].enabled);
    ui->temp_display_4->setEnabled( datalogger[3].enabled);
    ui->batt_display_4->setEnabled( datalogger[3].enabled);
    ui->temp_display_4->display(    datalogger[3].temp);
    ui->batt_display_4->display(    datalogger[3].batt);

    ui->     temp_pb_5->setValue(   datalogger[4].temp);
    ui->     batt_pb_5->setValue(   datalogger[4].batt);
    ui->     temp_pb_5->setEnabled( datalogger[4].enabled);
    ui->     batt_pb_5->setEnabled( datalogger[4].enabled);
    ui->temp_display_5->setEnabled( datalogger[4].enabled);
    ui->batt_display_5->setEnabled( datalogger[4].enabled);
    ui->temp_display_5->display(    datalogger[4].temp);
    ui->batt_display_5->display(    datalogger[4].batt);

    ui->     temp_pb_6->setValue(   datalogger[5].temp);
    ui->     batt_pb_6->setValue(   datalogger[5].batt);
    ui->     temp_pb_6->setEnabled( datalogger[5].enabled);
    ui->     batt_pb_6->setEnabled( datalogger[5].enabled);
    ui->temp_display_6->setEnabled( datalogger[5].enabled);
    ui->batt_display_6->setEnabled( datalogger[5].enabled);
    ui->temp_display_6->display(    datalogger[5].temp);
    ui->batt_display_6->display(    datalogger[5].batt);

    ui->     temp_pb_7->setValue(   datalogger[6].temp);
    ui->     batt_pb_7->setValue(   datalogger[6].batt);
    ui->     temp_pb_7->setEnabled( datalogger[6].enabled);
    ui->     batt_pb_7->setEnabled( datalogger[6].enabled);
    ui->temp_display_7->setEnabled( datalogger[6].enabled);
    ui->batt_display_7->setEnabled( datalogger[6].enabled);
    ui->temp_display_7->display(    datalogger[6].temp);
    ui->batt_display_7->display(    datalogger[6].batt);

}
////HELPING FUNCTIONS
void MainWindow::sector_handler(){
    segments_drawing = false;
    int node_order[n];
    node_order[0] = ui->sector_RSU_sp_1->value();
    node_order[1] = ui->sector_RSU_sp_2->value();
    node_order[2] = ui->sector_RSU_sp_3->value();
    node_order[3] = ui->sector_RSU_sp_4->value();
    node_order[4] = ui->sector_RSU_sp_5->value();
    node_order[5] = ui->sector_RSU_sp_6->value();
    node_order[6] = ui->sector_RSU_sp_7->value();
    node_order[7] = ui->sector_RSU_sp_8->value();

    for (int i=0 ; i<n ; i++){
        for (int j=i+1 ; j<n ; j++){
            if(node_order[i] == node_order[j] && node_order[j]!= 0){
                ui->status->setText("Order ERROR!!!");
                QWidget::update();
                return;
            }
        }
    }

    if(node_order[0] == 0) return;
    seg_table[0].node_id_1 = node_order[0]; //set the first value
    for (int i=1 ; i<n ; i++){
        if(node_order[i] == 0) break;
        seg_table[i-1].node_id_2 = node_order[i];
        seg_table[i].node_id_1 = node_order[i];
        if(node_order[i] == 0) seg_table[i-1].draw = false;
        else seg_table[i-1].draw = true;
    }
    qDebug()<<"--Segment Table";
    for (int i=0 ; i<n ; i++){
        qDebug()<<"Sec:"<<i<<" -N1: "<<seg_table[i].node_id_1<<" -N2: "<<seg_table[i].node_id_2;
    }
    segments_drawing = true;
    QWidget::update();
    ui->status->setText("Order Succesful!!!");
}
void MainWindow::sector_count_handler(){
    for (int i=0 ; i<n-1 ; i++){
        if (seg_table[i].node_id_1 == -1) break;
        seg_table[i].count = datalogger[seg_table[i].node_id_1-2].count - datalogger[seg_table[i].node_id_2-2].count;
        if(seg_table[i].node_id_2 == seg_table[0].node_id_1) seg_table[i].count++;
        //if(seg_table[i].count < 0) seg_table[i].count = 0;
    }
    QWidget::update();
    qDebug() <<"--DATALOGGER-";
    qDebug() <<"N2"<<datalogger[0].count<<"N3"<<datalogger[1].count<<"N4"<<datalogger[2].count<<"N5"<<datalogger[3].count;
}
void MainWindow::segments_counter(int source_node, int counter){
    for (int i=0 ; i<n-1 ; i++){
        //search for the pos of the source node, pos=i
        if (seg_table[i].node_id_1 == source_node){
            seg_table[i].flash = true;
            seg_table[i].count = counter;
            //the node_id_2 has the previous node to decrement
            //find pos of the node to decrement and decrement the node
            for (int c=0 ; c<n-1 ; c++) {
                if(seg_table[c].node_id_2 == seg_table[i].node_id_1) {
                    seg_table[c].zero_point++;
                    if(seg_table[c].count-seg_table[c].zero_point <= 0 ) seg_table[c].count=seg_table[c].zero_point;
                }
            }
        }
    }
}
QString MainWindow::int_to_qstring(int x){
    return QString::number(x);
}
////TESTING-DEBUGING
void MainWindow::on_test_button_clicked()
{
    collision_timer->start(250);
    collision_drawing = true;
    ui->clear_warning_button->setEnabled(true);
    selfID = 4;
    int offset = 0;
    RSU_table[0] = {1, offset+60,  offset+60, 2,1,0, true};
    RSU_table[1] = {2, offset+150, offset+25, 3,1,0, true};
    RSU_table[2] = {3, offset+240, offset+60, 4,1,0, true};
    RSU_table[3] = {4, offset+270, offset+140, 4,0,0, true};
    RSU_table[4] = {5, offset+240, offset+240, 4,1,0, true};
    RSU_table[5] = {6, offset+150, offset+270, 7,2,0, true};
    RSU_table[6] = {7, offset+60,  offset+245, 5,1,0, true};
    RSU_table[7] = {8, offset+25,  offset+140, 7,3,0, true};
    routes_drawing = true;
    ui->clear_warning_button->setEnabled(true);
    ui->MU_message_cb->setEnabled(true);
    ui->serial_MUsend_button->setEnabled(true);
    collision_MU_node.id  = 8;
    collision_MU_node.edge_dest=6;
    collision_MU_node.draw = true;
    collision_timer->start(500);
    QWidget::update();
    qDebug()<<"Test 1 done!!!";
}
void MainWindow::on_test_button_2_clicked()
{
    segments_init();
    segments_drawing = true;
    for (int i=0; i<n-1 ; i++) seg_table[i].draw=true;
    seg_table[5].flash = true;
    QWidget::update();
    qDebug()<<"Test2!!";
}
void MainWindow::on_test_button_3_clicked()
{
    datalogger[0].enabled=true;
    datalogger[0].temp = 21.6;
    datalogger[0].batt = 3.7;
    datalogger_update();
    datalogger_timer->start(1000);
    ui->clear_warning_button->setEnabled(true);
    ui->MU_message_cb->setEnabled(true);
    ui->serial_MUsend_button->setEnabled(true);
    collision_MU_node.id  =13;
    collision_MU_node.edge_dest=6;
    collision_MU_node.draw = true;
    collision_timer->start(500);
    QWidget::update();
    qDebug()<<"Test3";
}
