﻿#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "sampletable.h"
#include "GCLHighLighter.h"
#include "QProcess"
#include "QLibrary"
#include <QTextStream>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
//#include "C:\\Users\\ktd43279\\Documents\\PROGS\\MaxSCriptMaker_laptop\\include\\genie_data_access.h"
#include <iostream>
#include <QHostInfo>
#include <QDesktopServices>
#include "ScriptLines.h"
#include <QFile>
#include <QTextStream>
#include <QLineEdit>
#include <QStandardPaths>
#include <QSettings>
#include "pyhighlighter.h"
#include <QProgressBar>
#include <QTimer>
#include <QSize>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(0);

    fileName = "";
    ui->saveButton->setEnabled(false);

    //Script is in OpenGenie at launch
    ui->OGButton->setChecked(true);
    OGhighlighter = new Highlighter(ui->plainTextEdit->document());
    pyhighlighter = new KickPythonSyntaxHighlighter(ui->PyScriptBox->document());

    mySampleTable = new SampleTable(); // Be sure to destroy this window somewhere
    initMainTable();
    ProgressBar(10, 1);//EVENTUALLY BELONGS SOMEWHERE ELSE

    connect(ui->tableWidget_1,SIGNAL(currentCellChanged(int,int,int,int)),SLOT(parseTableSlot()));
    connect(mySampleTable,SIGNAL(closedSampWindow()), SLOT(disableRows()));

    parseTable();

}


void MainWindow::initMainTable(){

    actions << " " << "Run" << "Run with SM" << "Kinetic run" << "Run PNR" << "Run PA" \
            << "Free text (OG)"\
            << "--------------"<< "contrastchange" << "Set temperature" << "NIMA" \
            << "--------------"<< "Set Field" << "Run Transmissions";


    QList<QTableWidget*> tables = this->findChildren<QTableWidget*>(); //finds all children of type QTableWidget

    //save number of rows/cols for the 0th entry in qlist, would probs be main table
    const int ROWS = tables[0]->rowCount();
    const int COLS = tables[0]->columnCount();

    int i=0;

    foreach (QTableWidget *table, tables)
    {
        for (int r = 0; r < ROWS; r++) {
            QComboBox *combo = new QComboBox(); //combobox on each line
            //combo->setParent(table);
            table->setCellWidget (r, 0, combo); //combobox in row r and column 0
            combo->addItems(actions);           //puts runoptions in combobox
            combo->setProperty("row", r);   //sets the row property to hold row number r
            connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(onRunSelected(int)));
            i++;
            combo->setDisabled(mySampleTable->sampleList.isEmpty());
        }
        for (int row = 0; row< ROWS; row++){
            for (int col = 1; col< COLS; col++){

                //if there is a combobox in a cell remove the cell widget?
                if(qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,col)))
                    ui->tableWidget_1->removeCellWidget(row,col);
                QTableWidgetItem *newItem = new QTableWidgetItem;
                newItem->setText("");
                table->setItem(row,col,newItem); //puts/replaces an empty cell where the widget was removed

            }
        }

        //don't these two commands work against eachother
        table->resizeColumnToContents(0);
        table->setColumnWidth(10,40);
    }

    disableRows();

    connect(ui->pushButton_3, SIGNAL(clicked()), this, SLOT(openSampleTable()));
    connect(ui->playButton, SIGNAL(clicked()), this, SLOT(runGenie())); //PLAY BUTTON
    ui->tableWidget_1->setColumnWidth(0,120);

    //Shows Context Menu
    ui->tableWidget_1->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableWidget_1->verticalHeader(), SIGNAL(customContextMenuRequested(const QPoint&)),
        this, SLOT(ShowContextMenu(const QPoint&)));

    // initialise run time to 0:
    QTime time = QTime::fromString("00:00", "hh:mm");
    runTime = time;

}

void MainWindow::disableRows(){

    QList<QComboBox*> boxes = ui->tableWidget_1->findChildren<QComboBox*>();
    bool disable;
    if (mySampleTable->sampleList.isEmpty()) disable = true;
    else disable = false;

    for (int row = 0; row < 100; row++){
        QComboBox *box = boxes[row];
        box->setDisabled(disable);

        auto currentFlags = ui->tableWidget_1->item(row,10)->flags();
        ui->tableWidget_1->item(row,10)->setFlags(currentFlags & (~Qt::ItemIsEditable));  //permanently disable column 10 for progressbars

        for (int col = 1; col < 10; col++){
            currentFlags = ui->tableWidget_1->item(row,col)->flags();
            if (disable) {
                ui->tableWidget_1->item(row,col)->setFlags(currentFlags & (~Qt::ItemIsEditable));
            }
            else {
                ui->tableWidget_1->item(row,col)->setFlags(currentFlags | Qt::ItemIsEditable);
            }
        }
    }
}



void MainWindow::parseTableSlot(){
    parseTable();
}


void MainWindow::writeBackbone(){

    ui->plainTextEdit->clear();
    pyWriteBackbone();

    QString BFileName;
    //Get Backbone from .txt file
    if (ui->OGButton->isChecked())
        BFileName = ":/OGbackbone.txt";
    else if (ui->PythonButton->isChecked())
        BFileName = ":/PyBackbone.txt";

    QFile BFile(BFileName);

    if (!BFile.open(QFile::ReadOnly | QFile::Text)){
           QMessageBox::warning(this, "Error" , "Couldn't open OpenGenie Backbone File");
    }

    QTextStream in(&BFile);
    QString OGtext = in.readAll();
    ui->plainTextEdit->setPlainText(OGtext);

    BFile.close();


    ui->plainTextEdit->find("GLOBAL runTime"); //positions the cursor to insert instructions
    ui->plainTextEdit->moveCursor(QTextCursor::EndOfLine, QTextCursor::MoveAnchor);
    for (int i=0; i < mySampleTable->sampleList.length(); i++){
        ui->plainTextEdit->insertPlainText(" s" + QString::number(i+1)); // sample numbering starts with 1
    }
}

void MainWindow::pyWriteBackbone(){

    ui->PyScriptBox->clear();
    QString BFileName;
    BFileName = ":/PyBackbone.txt";

    QFile BFile(BFileName);

    if (!BFile.open(QFile::ReadOnly | QFile::Text)){
           QMessageBox::warning(this, "Error" , "Couldn't open Python Backbone File");
    }

    QTextStream in(&BFile);
    QString Pytext = in.readAll();
    ui->PyScriptBox->setPlainText(Pytext);

    BFile.close();

    ui->PyScriptBox->find("def runscript()"); //positions the cursor to insert instructions
    ui->PyScriptBox->moveCursor(QTextCursor::Down, QTextCursor::MoveAnchor);
}

void MainWindow::parseTable(){

    //finds all comboboxes that are children of the table
    //this won't include other comboxes which are themselves children of comboboxes
    QList<QComboBox*> combo = ui->tableWidget_1->findChildren<QComboBox*>();
    QString scriptLine; // a string that temporarily stores info before adding to script
    int sendingRow;

    // change column headers according to row being edited
    sendingRow = ui->tableWidget_1->currentRow();
    if(sendingRow>-1)
        setHeaders(combo[sendingRow]->currentIndex());

    // initialise run time to 0:
    runTime = runTime.fromString("00:00", "hh:mm");

    // prepare script header
    writeBackbone();

    ui->plainTextEdit->find("runTime=0"); //positions the cursor to insert instructions
    ui->plainTextEdit->moveCursor(QTextCursor::Down, QTextCursor::MoveAnchor);

    // process each table row
    for (int row=0; row < ui->tableWidget_1->rowCount(); row++){
        //ui->tableWidget_1->item(row, 8)->setBackground(Qt::white);
        int whatAction = combo[row]->currentIndex();
        switch(whatAction)
        {
            case 0:
                break;
            case 1:
                normalRun(row, false);
                break;
            case 2: // run with supermirror
               { normalRun(row, true);}
                break;
            case 3:// run kinetic
                kineticRun(row);
                break;
            case 6: // free OpenGenie command
                OGcommand(row);
                break;
            case 8: // contrastChange
                {contrastChange(row);
                break;}
            case 9:// set temperature
                setTemp(row);
                break;
            case 10: // NIMA control
                setNIMA(row);
                break;
            case 13: // run transmissions
                runTrans(row);
                break;
        }
    }
    samplestoPlainTextEdit();
    if(ui->checkBox->isChecked()){
        save(OPENGENIE);
    }
    if(ui->PySaveCheckBox->isChecked())
        save(PYTHON);

}
//------------------------------------------------------------------------------------------------------------------//
//------------------------------------------------------------------------------------------------------------------//
//---------------------------------RUNOPTIONS-----------------------------------------------------------------------//
//------------------------------------------------------------------------------------------------------------------//
void MainWindow::samplestoPlainTextEdit(){
    ui->plainTextEdit->find("#do not need to be changed during experiment."); //positions the cursor to insert instructions
    ui->plainTextEdit->moveCursor(QTextCursor::Down, QTextCursor::MoveAnchor);
    ui->plainTextEdit->moveCursor(QTextCursor::Down, QTextCursor::MoveAnchor); //move cursor down one more line
    QList<NRSample> samples = mySampleTable->sampleList;
    ui->plainTextEdit->insertPlainText(writeSamples(samples));
}

void MainWindow::normalRun(int row, bool runSM){

    bool ok;
    QComboBox* whichSamp = new QComboBox;
    runstruct runvars;

    if(mySampleTable->sampleList.length()){ //if there is a sample

            whichSamp = (QComboBox*)ui->tableWidget_1->cellWidget(row, 1);
            runvars.sampName = mySampleTable->sampleList[whichSamp->currentIndex()].title;

            mySampleTable->sampleList[whichSamp->currentIndex()].subtitle = ui->tableWidget_1->item(row,2)->text();
            runvars.subtitle = ui->tableWidget_1->item(row,2)->text();

            runvars.sampNum = QString::number(whichSamp->currentIndex()+1);

            for(int cell = 0; cell < 3; cell++){
                ok = false;
                runvars.angles[cell] = (ui->tableWidget_1->item(row,2*cell+3)->text()).toDouble(&ok);
                runvars.uAmps[cell] = (ui->tableWidget_1->item(row,2*cell+4)->text()).toDouble(&ok);

                if (runvars.angles[cell] > 0.0 && runvars.uAmps[cell] > 0.0) // apply filter on 'reasonable' angles and run times
                {
                    ok = true;
                    updateRunTime(runvars.uAmps[cell]);
                } else if  (!(ui->tableWidget_1->item(row,2*cell+3)->text().contains("Angle")\
                            && ui->tableWidget_1->item(row,2*cell+4)->text().contains("uAmps"))\
                        || !(ui->tableWidget_1->item(row,2*cell+3)->text() == ""\
                                && ui->tableWidget_1->item(row,2*cell+4)->text() == ""))
                    {
                        ok = false; break;
                    }
            };

            if (ok){
                ui->tableWidget_1->item(row, 10)->setBackground(Qt::green);
            } else {
                ui->tableWidget_1->item(row, 10)->setBackground(Qt::red);
            }
        }

    QString scriptText;
    scriptText = writeRun(runvars, runSM, false);
    ui->plainTextEdit->insertPlainText(scriptText);
    ui->PyScriptBox->insertPlainText(writeRun(runvars, runSM, true));
    return;
}

void MainWindow::kineticRun(int row){

    QComboBox* whichSamp = new QComboBox;
    QString scriptLine;

    whichSamp=(QComboBox*)ui->tableWidget_1->cellWidget(row, 1);
    scriptLine = "runTime = runKinetic(s" + QString::number(whichSamp->currentIndex()+1) \
            + "," + ui->tableWidget_1->item(row,3)->text() + "," \
            + ui->tableWidget_1->item(row,4)->text() + ")";
    ui->plainTextEdit->insertPlainText(scriptLine+ "\n");
    return;
}

void MainWindow::OGcommand(int row){

      ui->plainTextEdit->insertPlainText(ui->tableWidget_1->item(row,1)->text()+ "\n");
}

void MainWindow::contrastChange(int row){

    int percentSum = 0;
    int secs;
    double angle1;
    bool ok, wait;
    QString OGscriptLine, PyScriptLine;
    QComboBox* whichSamp = new QComboBox;
    QComboBox* continueRun = new QComboBox;

    runstruct runvars;

    whichSamp = (QComboBox*)ui->tableWidget_1->cellWidget(row, 1);
    continueRun = (QComboBox*)ui->tableWidget_1->cellWidget(row, 8);

    // check if A-D are integers and sum to 100:
    for (int i=0; i < 4; i++){
        runvars.concs[i] = (ui->tableWidget_1->item(row,i+2)->text()).toInt(&ok);

        if (runvars.concs[i] >= 0){
            percentSum += runvars.concs[i];
        } else {
            percentSum = 0;
            break;
        }
    }

    runvars.flow = (ui->tableWidget_1->item(row,6)->text()).toDouble();
    runvars.volume = (ui->tableWidget_1->item(row,7)->text()).toDouble();
    runvars.knauer = mySampleTable->sampleList[whichSamp->currentIndex()].knauer;

    if (percentSum != 100 || runvars.flow <= 0.0 || runvars.volume <= 0.0){
        ui->tableWidget_1->item(row, 10)->setBackground(Qt::red);
        return;
       }
    else ui->tableWidget_1->item(row, 10)->setBackground(Qt::green);

    if (continueRun->currentIndex()){

        angle1 = runvars.volume/runvars.flow; //pump time in minutes
        secs = static_cast<int>(angle1*60); //for TS2 current
        ui->timeEdit->setTime(runTime.addSecs(secs));//whichSamp->currentIndex()+1)

        wait = true;
        OGscriptLine = writeContrast(runvars, wait, false);
        PyScriptLine = writeContrast(runvars, wait, true);

    }else {
        wait = false;
        OGscriptLine = writeContrast(runvars, wait, false);
        PyScriptLine = writeContrast(runvars, wait, true);
    }
    ui->plainTextEdit->insertPlainText(OGscriptLine + "\n");
    ui->PyScriptBox->insertPlainText(PyScriptLine + "\n");
    return;
}

void MainWindow::setTemp(int row){

    QString scriptLine = "";
    bool ok;
    QComboBox* whichTemp = new QComboBox;
    QComboBox* runControl = new QComboBox;
    runstruct runvars;

    ui->tableWidget_1->item(row, 10)->setBackground(Qt::red);

    whichTemp = (QComboBox*)ui->tableWidget_1->cellWidget(row, 1);
    switch (whichTemp->currentIndex())
    {
        case 0:{ //Julabo control
            ok=true;
            runControl = (QComboBox*)ui->tableWidget_1->cellWidget(row, 3);
            //int runCont = 1;//TEMP
            runvars.JTemp = ui->tableWidget_1->item(row,2)->text().toDouble(&ok);

            int runCont;
            if (runvars.JTemp > -5.0 && runvars.JTemp < 95.0 && ok)
                    runCont = runControl->currentIndex();

            if (runCont && ok){
                runvars.JMin = ui->tableWidget_1->item(row,4)->text().toDouble(&ok);
                runvars.JMax = ui->tableWidget_1->item(row,5)->text().toDouble(&ok);

                if (runCont && runvars.JMax > runvars.JTemp && runvars.JMin < runvars.JTemp){
                    scriptLine = writeJulabo(runvars, runCont);
                    ui->tableWidget_1->item(row, 10)->setBackground(Qt::green);
                }
             }

            else if (!runCont && ok) {
                scriptLine = writeJulabo(runvars, runCont);
                ui->tableWidget_1->item(row, 10)->setBackground(Qt::green);
            }

            break;}
    case 1:{ // Eurotherms control
            ok = true;
            int i = 0;

            while(i < 9 && ok){
                runvars.euroTemps[i] = (ui->tableWidget_1->item(row,i+2)->text()).toDouble(&ok);
                i++;
            }
            if (ok) ui->tableWidget_1->item(row, 10)->setBackground(Qt::green);
            scriptLine = writeEuro(runvars);

            break;}
    case 2:{ // Peltier control
            break;}
    }

    ui->plainTextEdit->insertPlainText(scriptLine + "\n");
    return;
}

void MainWindow::setNIMA(int row){

      QString OGscriptLine, PyScriptLine;
      QComboBox* box1 = new QComboBox;
      bool Pressure, ok;
      runstruct runvars;

      box1 = (QComboBox*)ui->tableWidget_1->cellWidget(row, 1);
      ui->tableWidget_1->item(row, 10)->setBackground(Qt::red);

      if(box1->currentText().contains("Pressure")){
          Pressure = true;
          runvars.pressure = (ui->tableWidget_1->item(row,2)->text()).toDouble(&ok);
       }

      else if (box1->currentText().contains("Area"))
          Pressure = false;
          runvars.area = (ui->tableWidget_1->item(row,2)->text()).toDouble(&ok);

      if (ok) {
          OGscriptLine = writeNIMA(runvars, Pressure, false);
          PyScriptLine = writeNIMA(runvars, Pressure, true);
          ui->tableWidget_1->item(row, 10)->setBackground(Qt::green);
      }

      ui->plainTextEdit->insertPlainText(OGscriptLine + "\n");
      ui->PyScriptBox->insertPlainText(PyScriptLine);
      return;
}

void MainWindow::runTrans(int row){

    double angle2;
    bool ok;
    QComboBox* whichSamp = new QComboBox;
    runstruct runvars;

    if(mySampleTable->sampleList.length()){
        whichSamp=(QComboBox*)ui->tableWidget_1->cellWidget(row, 1);
        runvars.sampName = mySampleTable->sampleList[whichSamp->currentIndex()].title;

        mySampleTable->sampleList[whichSamp->currentIndex()].subtitle = ui->tableWidget_1->item(row,2)->text();
        runvars.subtitle = ui->tableWidget_1->item(row,2)->text();

        runvars.sampNum = QString::number(whichSamp->currentIndex()+1);
        runvars.heightOffsT = (ui->tableWidget_1->item(row,3)->text()).toDouble(&ok);

        for (int i = 0; i < 3; i++){
            runvars.angles[i] = (ui->tableWidget_1->item(row,i+4)->text()).toDouble(&ok);
        }
        runvars.uAmpsT = (ui->tableWidget_1->item(row,7)->text()).toDouble(&ok);
        angle2 = (ui->tableWidget_1->item(row,8)->text()).toDouble(&ok);

        if (ok){
            ui->tableWidget_1->item(row, 10)->setBackground(Qt::green);
            updateRunTime(angle2);

            ui->plainTextEdit->insertPlainText(writeTransm(runvars, false) + "\n");
            ui->PyScriptBox->insertPlainText(writeTransm(runvars, true) + "\n");

        } else ui->tableWidget_1->item(row, 10)->setBackground(Qt::red);

    }

    return;
}

//-----------------------------------------RUNOPTIONS OVER----------------------------------------------------------//
//------------------------------------------------------------------------------------------------------------------//
//------------------------------------------------------------------------------------------------------------------//
//------------------------------------------------------------------------------------------------------------------//
//------------------------------------------------------------------------------------------------------------------//
void MainWindow::updateRunTime(double angle){
    int secs;
    if(ui->instrumentCombo->currentText() == "CRISP" || ui->instrumentCombo->currentText() == "SURF"){
        secs = static_cast<int>(angle/160*3600); // TS2
    } else {
        secs = static_cast<int>(angle/40*3600); // TS2
    }
    runTime = runTime.addSecs(secs);
    ui->timeEdit->setTime(runTime);
    return;
}


void MainWindow::openSampleTable()
{
//    if(mySampleTable->currentSample > 0){
        mySampleTable->displaySamples();
    mySampleTable->show();


}



void MainWindow::updateSubtitleSlot(){
    /*QComboBox *comb = (QComboBox *)sender();
    int nRow = comb->property("row").toInt();
    ui->tableWidget_1->item(nRow,2)->setText(mySampleTable->sampleList[comb->currentIndex()].subtitle);
    */
    parseTable();
}



//changes the table when we select run, run transmissions, contrast change
void MainWindow::setHeaders(int which){
      switch (which) {
    case 0: // empty
        for(int i=2; i<12; i++){
            ui->tableWidget_1->setHorizontalHeaderItem(i-1,new QTableWidgetItem(QString::number(i)));
        }
        break;
    case 1: // Run
        ui->tableWidget_1->setHorizontalHeaderItem(2,new QTableWidgetItem("Subtitle"));
        ui->tableWidget_1->setHorizontalHeaderItem(3,new QTableWidgetItem("Angle 1"));
        ui->tableWidget_1->setHorizontalHeaderItem(4,new QTableWidgetItem("uAmps 1"));
        ui->tableWidget_1->setHorizontalHeaderItem(5,new QTableWidgetItem("Angle 2"));
        ui->tableWidget_1->setHorizontalHeaderItem(6,new QTableWidgetItem("uAmps 2"));
        ui->tableWidget_1->setHorizontalHeaderItem(7,new QTableWidgetItem("Angle 3"));
        ui->tableWidget_1->setHorizontalHeaderItem(8,new QTableWidgetItem("uAmps 3"));
        break;
    case 8: // contrastChange
        ui->tableWidget_1->setHorizontalHeaderItem(2,new QTableWidgetItem("concA"));
        ui->tableWidget_1->setHorizontalHeaderItem(3,new QTableWidgetItem("concB"));
        ui->tableWidget_1->setHorizontalHeaderItem(4,new QTableWidgetItem("concC"));
        ui->tableWidget_1->setHorizontalHeaderItem(5,new QTableWidgetItem("concD"));
        ui->tableWidget_1->setHorizontalHeaderItem(6,new QTableWidgetItem("Flow [mL/min]"));
        ui->tableWidget_1->setHorizontalHeaderItem(7,new QTableWidgetItem("Volume [mL]"));
        break;
    case 13: // Run transmissions
        ui->tableWidget_1->setHorizontalHeaderItem(2,new QTableWidgetItem("Subtitle"));
        ui->tableWidget_1->setHorizontalHeaderItem(3,new QTableWidgetItem("height offset"));
        ui->tableWidget_1->setHorizontalHeaderItem(4,new QTableWidgetItem("s1vg"));
        ui->tableWidget_1->setHorizontalHeaderItem(5,new QTableWidgetItem("s2vg"));
        ui->tableWidget_1->setHorizontalHeaderItem(6,new QTableWidgetItem("s3vg"));
        ui->tableWidget_1->setHorizontalHeaderItem(7,new QTableWidgetItem("s4vg"));
        ui->tableWidget_1->setHorizontalHeaderItem(8,new QTableWidgetItem("uamps"));
        break;
    default:
        break;
    }
}


void MainWindow::runControl(int value){
    QComboBox *comb = (QComboBox *)sender();
    QTableWidget *tabl = (QTableWidget *)comb->parentWidget()->parent();
    int nRow = comb->property("row").toInt();

    switch(value)
    {
        case 0:
            tabl->item(nRow,4)->setText("");
            tabl->item(nRow,5)->setText("");
            break;
        case 1:
            tabl->item(nRow,4)->setText("MIN");
            tabl->item(nRow,5)->setText("MAX");
            break;
    }
}


void  MainWindow::onDeviceSelected(int value)
{
    QComboBox *comb = (QComboBox *)sender();
    QTableWidget *tabl = (QTableWidget *)comb->parentWidget()->parent();
    QComboBox* runControl = new QComboBox;
    QStringList yesNo;
    int nRow = comb->property("row").toInt();

    QComboBox* box = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(nRow,3));
    if(box)
        tabl->removeCellWidget(nRow,3); // remove run control box
    for(int i=1;i<10;i++){
        tabl->item(nRow,i)->setText("");
    }
    switch(value)
    {
        case 0:
            tabl->item(nRow,2)->setText("Julabo Temp");            
            yesNo << "no runcontrol" << "RUNCONTROL";
            runControl->addItems(yesNo);
            tabl->setCellWidget(nRow,3,runControl);
            runControl->setProperty("row", nRow);
            connect(runControl, SIGNAL(activated(int)), this, SLOT(runControl(int)));
            break;
        case 1:
            tabl->item(nRow,2)->setText("T1");
            tabl->item(nRow,3)->setText("T2");
            tabl->item(nRow,4)->setText("T3");
            tabl->item(nRow,5)->setText("T4");
            tabl->item(nRow,6)->setText("T5");
            tabl->item(nRow,7)->setText("T6");
            tabl->item(nRow,8)->setText("T7");
            break;
    }
}


void  MainWindow::onModeSelected(int value)
{
    QComboBox *comb = (QComboBox *)sender();
    QTableWidget *tabl = (QTableWidget *)comb->parentWidget()->parent();
    int nRow = comb->property("row").toInt();

    for(int i=1;i<10;i++){
        tabl->item(nRow,i)->setText("");
    }
    switch(value)
    {
        case 0:
            tabl->item(nRow,2)->setText("Target p");
            break;
        case 1:
            tabl->item(nRow,2)->setText("Target A");
            break;
    }
}


void  MainWindow::onRunSelected(int value)
{
    QComboBox *comb = (QComboBox *)sender();
    QTableWidget *tabl = (QTableWidget *)comb->parentWidget()->parent();

    int nRow = comb->property("row").toInt();
    for(int i=1;i < ui->tableWidget_1->columnCount();i++){
        tabl->item(nRow,i)->setText("");
    }
    QComboBox* comboTemp = new QComboBox();
    QStringList devices;
    devices << "Julabo Waterbath" << "Eurotherm 8x" << "Peltier x4";
    comboTemp->addItems(devices);

    QComboBox* combo = new QComboBox();
    QStringList modes;
    modes << "Pressure Ctrl" << "Area Ctrl";
    combo->addItems(modes);

    comb->setStyleSheet("QComboBox { background-color: lightGray; }");

    QComboBox* samplesCombo = new QComboBox();
    QString s;
    samples.clear();
    for (int i=0;i<mySampleTable->sampleList.length();i++){
        s = mySampleTable->sampleList[i].title;
        samples << s;
    }

    samplesCombo->addItems(samples);

    QComboBox* waitCombo = new QComboBox();
    QStringList wait;
    wait << "CONTINUE" << "WAIT";
    waitCombo->addItems(wait);

    tabl->removeCellWidget(nRow,1);
    tabl->removeCellWidget(nRow,3);
    tabl->removeCellWidget(nRow,8);
    setHeaders(0);

    switch(value)
    {
    case 1: // run
            if(mySampleTable->sampleList.length()){
                setHeaders(1);
                comb->setStyleSheet("QComboBox { background-color: lightGreen; }");
                tabl->removeCellWidget(nRow,1);
                tabl->setCellWidget (nRow, 1, samplesCombo);
                tabl->item(nRow,2)->setText(mySampleTable->sampleList[samplesCombo->currentIndex()].subtitle);
                tabl->item(nRow,2)->setToolTip("Subtitle");
                tabl->item(nRow,3)->setText("Angle 1");
                tabl->item(nRow,3)->setToolTip("Angle 1");
                tabl->item(nRow,4)->setText("uAmps 1");
                tabl->item(nRow,4)->setToolTip("uAmps 1");
                tabl->item(nRow,5)->setText("Angle 2");
                tabl->item(nRow,5)->setToolTip("Angle 2");
                tabl->item(nRow,6)->setText("uAmps 2");
                tabl->item(nRow,6)->setToolTip("uAmps 2");
                tabl->item(nRow,7)->setText("Angle 3");
                tabl->item(nRow,7)->setToolTip("Angle 3");
                tabl->item(nRow,8)->setText("uAmps 3");
                tabl->item(nRow,8)->setToolTip("uAmps 3");
                connect(samplesCombo, SIGNAL(activated(int)), this, SLOT(updateSubtitleSlot()));
            } else {
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setText("There are no samples to run. Please define at least one!");
                msgBox.exec();
            }
            break;
        case 2: // run with SM
            setHeaders(1);
            comb->setStyleSheet("QComboBox { background-color: lightGreen; }");
            tabl->removeCellWidget(nRow,1);
            tabl->setCellWidget (nRow, 1, samplesCombo);
            tabl->item(nRow,2)->setText(mySampleTable->sampleList[samplesCombo->currentIndex()].subtitle);
            tabl->item(nRow,2)->setToolTip("Subtitle");
            tabl->item(nRow,3)->setText("Angle 1");
            tabl->item(nRow,3)->setToolTip("Angle 1");
            tabl->item(nRow,4)->setText("uAmps 1");
            tabl->item(nRow,4)->setToolTip("uAmps 1");
            tabl->item(nRow,5)->setText("Angle 2");
            tabl->item(nRow,5)->setToolTip("Angle 2");
            tabl->item(nRow,6)->setText("uAmps 2");
            tabl->item(nRow,6)->setToolTip("uAmps 2");
            tabl->item(nRow,7)->setText("Angle 3");
            tabl->item(nRow,7)->setToolTip("Angle 3");
            tabl->item(nRow,8)->setText("uAmps 3");
            tabl->item(nRow,8)->setToolTip("uAmps 3");
            connect(samplesCombo, SIGNAL(activated(int)), this, SLOT(updateSubtitleSlot()));
            break;
        case 3: // run kinetic
            comb->setStyleSheet("QComboBox { background-color: lightGreen; }");
            tabl->removeCellWidget(nRow,1);
            tabl->setCellWidget (nRow, 1, samplesCombo);
            tabl->item(nRow,3)->setText("Angle");
            tabl->item(nRow,3)->setToolTip("Angle");
            tabl->item(nRow,4)->setText("time");
            tabl->item(nRow,4)->setToolTip("time per step");
            tabl->item(nRow,5)->setText("No of steps");
            tabl->item(nRow,4)->setToolTip("No of steps");
            connect(samplesCombo, SIGNAL(activated(int)), this, SLOT(parseTableSlot()));
            break;
        case 8: // contrastChange
            setHeaders(8);
            tabl->removeCellWidget(nRow,1);
            tabl->setCellWidget (nRow, 1, samplesCombo);
            tabl->item(nRow,2)->setText("conc A");
            tabl->item(nRow,2)->setToolTip("conc A");
            tabl->item(nRow,3)->setText("conc B");
            tabl->item(nRow,3)->setToolTip("conc B");
            tabl->item(nRow,4)->setText("conc C");
            tabl->item(nRow,4)->setToolTip("conc C");
            tabl->item(nRow,5)->setText("conc D");
            tabl->item(nRow,5)->setToolTip("conc D");
            tabl->item(nRow,6)->setText("Flow[mL/min]");
            tabl->item(nRow,6)->setToolTip("Flow[mL/min]");
            tabl->item(nRow,7)->setText("Volume[mL]");
            tabl->item(nRow,6)->setToolTip("Volume[mL]");
            tabl->setCellWidget (nRow, 8, waitCombo);
            connect(samplesCombo, SIGNAL(activated(int)), this, SLOT(Slot()));
            connect(waitCombo, SIGNAL(activated(int)), this, SLOT(parseTableSlot()));
            break;
        case 9: // set temperature
            comb->setStyleSheet("QComboBox { background-color: orange; }");
            tabl->setCellWidget (nRow, 1, comboTemp);
            comboTemp->setProperty("row", nRow);
            connect(comboTemp, SIGNAL(currentIndexChanged(int)), this, SLOT(onDeviceSelected(int)));
            connect(comboTemp, SIGNAL(activated(int)), this, SLOT(onDeviceSelected(int)));
            //comboTemp->setCurrentIndex(1);
            //comboTemp->setCurrentIndex(0);
            break;
        case 10: // NIMA control
            tabl->setCellWidget (nRow, 1, combo);
            combo->setProperty("row", nRow);
            connect(combo, SIGNAL(activated(int)), this, SLOT(onModeSelected(int)));
            break;
        case 13: // Run Transmissions
            setHeaders(13);
            tabl->removeCellWidget(nRow,1);
            tabl->setCellWidget (nRow, 1, samplesCombo);
            tabl->item(nRow,2)->setText("Subtitle");
            tabl->item(nRow,2)->setToolTip("Subtitle");
            tabl->item(nRow,3)->setText("height offset");
            tabl->item(nRow,3)->setToolTip("height offset");
            tabl->item(nRow,4)->setText("s1vg");
            tabl->item(nRow,4)->setToolTip("s1vg");
            tabl->item(nRow,5)->setText("s2vg");
            tabl->item(nRow,5)->setToolTip("s2vg");
            tabl->item(nRow,6)->setText("s3vg");
            tabl->item(nRow,6)->setToolTip("s3vg");
            tabl->item(nRow,7)->setText("s4vg");
            tabl->item(nRow,7)->setToolTip("s4vg");
            tabl->item(nRow,7)->setText("uAmps");
            tabl->item(nRow,7)->setToolTip("uAmps");
            connect(samplesCombo, SIGNAL(activated(int)), this, SLOT(parseTableSlot()));
            break;
    }

    parseTable();
    //emit valueChanged(value);
}

//-------------------------------------------------------------------------------------------------------------//
//-------------------------------------------------------------------------------------------------------------//
//-----------------------------------COPY CUT PASTE ETC--------------------------------------------------------//
//-------------------------------------------------------------------------------------------------------------//

//this is a right-click
void MainWindow::ShowContextMenu(const QPoint& pos) // this is a slot
{
    // for most widgets
    QPoint globalPos = ui->tableWidget_1->mapToGlobal(pos);
    // for QAbstractScrollArea and derived classes you would use:
    // QPoint globalPos = myWidget->viewport()->mapToGlobal(pos);

    QMenu myMenu;
    myMenu.addAction(new QAction("Insert Empty Row", this));
    //connect(newAction, SIGNAL(triggered()), this, SLOT(insertEmptyRow()));
    myMenu.addAction(new QAction("Delete Row", this));
    // ...

    QAction* selectedItem = myMenu.exec(globalPos);
    if (selectedItem)
    {
        int row = ui->tableWidget_1->row(ui->tableWidget_1->itemAt(pos));
        ui->tableWidget_1->item(2,2)->setText(QString::number(row));
        // something was chosen, do stuff
    }
    else
    {
        // nothing was chosen
    }
}

QTableWidgetSelectionRange MainWindow::selectedRange() const
{
    QList<QTableWidgetSelectionRange> ranges = ui->tableWidget_1->selectedRanges();
    if (ranges.isEmpty())
        return QTableWidgetSelectionRange();
    return ranges.first();
}

//copies table content to clipboard for pasting
void MainWindow::on_actionCopy_triggered()
{
    QTableWidgetSelectionRange range = selectedRange();
    QString str;

    for (int i = 0; i < range.rowCount(); ++i) {
        if (i > 0)
            str += "\n";
        for (int j = 0; j < range.columnCount(); ++j) {
            if (j > 0)
                str += "\t";
            str += ui->tableWidget_1->item(range.topRow() + i, range.leftColumn() + j)->text();
        }
    }
    QApplication::clipboard()->setText(str);

}


//let's user paste into other row/column
void MainWindow::on_actionPaste_triggered()
{
    QTableWidgetSelectionRange range = selectedRange();
    QString str = QApplication::clipboard()->text();
    QStringList rows = str.split('\n');
    int numRows = rows.count();
    int numColumns = rows.first().count('\t') + 1;

    if (range.rowCount() * range.columnCount() != 1
            && (range.rowCount() != numRows
                || range.columnCount() != numColumns)) {

        /*QMessageBox::information(this, tr("Spreadsheet"),
                tr("The information cannot be pasted because the copy "
                   "and paste areas aren't the same size."));
                   */
        return;
    }

    for (int i = 0; i < numRows; ++i) {
        QStringList columns = rows[i].split('\t');
        for (int j = 0; j < numColumns; ++j) {
            int row = range.topRow() + i;
            int column = range.leftColumn() + j;
            if (row < ui->tableWidget_1->rowCount() && column < ui->tableWidget_1->columnCount())
                ui->tableWidget_1->item(row, column)->setText(columns[j]);
        }
    }
    //somethingChanged();
}

//lets user delete
void MainWindow::on_actionDelete_triggered()
{
    QList<QTableWidgetItem *> items = ui->tableWidget_1->selectedItems();
    if (!items.isEmpty()) {
        foreach (QTableWidgetItem *item, items)
            delete item;
        //somethingChanged();
    }
}

//Copies and Deletes at the same time
void MainWindow::on_actionCut_triggered()
{
    on_actionCopy_triggered();
    on_actionDelete_triggered();
}



//??saves parameter in NRSample struct, don't get what it is doing to contrast change and temperature information
void MainWindow::on_actionOpen_Script_triggered()
{
    int line_count=0;
    int tableStart;
    QStringList sampleParameters;
    QStringList rowEntries;
    QString line[150];

    QString defaultLocation = QStandardPaths::locate(QStandardPaths::DesktopLocation, QString(), QStandardPaths::LocateDirectory);
    fileName = QFileDialog::getOpenFileName(this,tr("Open Script"), \
                                           defaultLocation, tr("Script files (*.scp)"));
    if (fileName!=""){
        QFile file(fileName);
        file.open(QIODevice::ReadOnly);
        QTextStream in(&file);
        //QString line = in.readAll();
        while(!in.atEnd())
        {
            line[line_count]=in.readLine(); //line Qstring temporarily stores file content
            if (line[line_count].contains("#TABLE"))
                    tableStart = line_count;
            line_count++;
        }
        file.close();
        QTextCharFormat boldFormat;
        boldFormat.setFontWeight(QFont::Bold);
        setWindowTitle("ScriptMax - " + file.fileName());
        for(int l=1; l < tableStart; l++){
            sampleParameters = line[l].split(','); //sampleParameters stores the parameters that are listed at the start of line Qstring
            if (sampleParameters.length() == 9){   //these parameters are then passed to a NRSample struct
                mySampleTable->currentSample = l-1;
                NRSample newSample;
                newSample.title = sampleParameters[0];
                newSample.translation = sampleParameters[1].toDouble();
                newSample.height = sampleParameters[2].toDouble();
                newSample.phi_offset = sampleParameters[3].toDouble();
                newSample.footprint = sampleParameters[4].toDouble();
                newSample.resolution = sampleParameters[5].toDouble();
                newSample.s3 = sampleParameters[6].toDouble();
                newSample.s4 = sampleParameters[7].toDouble();
                newSample.knauer = sampleParameters[8].toInt();
                mySampleTable->sampleList.append(newSample);
            } else {
                // pop up some error message
            }
        }

        //after the parameters the tablestarts, this info is now read
        //??don't really understand what this is doing
        for(int row=0; row < line_count; row++ ){
            QComboBox* box1 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,0));
            rowEntries = line[row+tableStart+1].split(',');
            int index1 = box1->findText(rowEntries[0]); //findText returns index of item containing whatever is at rowEntries[0]
            if(index1>0){
                box1->setCurrentIndex(index1);
                if(box1->currentText() == "contrastchange"){
                    QComboBox* box2 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,1));
                    box2->setCurrentIndex(box2->findText(rowEntries[1]));
                    for(int col = 2; col < 8; col++){
                        ui->tableWidget_1->item(row,col)->setText(rowEntries[col]);
                    }
                    QComboBox* box3 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,8));
                    int index2 = box3->findText(rowEntries[8]);
                    box3->setCurrentIndex(index2);
                } else if(box1->currentText().contains("Set temperature")\
                          && rowEntries[1] == "Julabo Waterbath"){
                    QComboBox* box2 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,1));
                    box2->setCurrentIndex(1);
                    box2->setCurrentIndex(0);
                    ui->tableWidget_1->item(row,2)->setText(rowEntries[2]);
                    QComboBox* box4 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,3));
                    if (box4){
                        int index2 = box4->findText(rowEntries[3]);
                        box4->setCurrentIndex(index2);
                    }
                    for(int col = 4; col < 9; col++){
                        ui->tableWidget_1->item(row,col)->setText(rowEntries[col]);
                    }

                } else if(rowEntries.length()){
                    QComboBox* box2 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,1));
                    box2->setCurrentIndex(box2->findText(rowEntries[1]));
                    for(int col = 2; col < 9; col++){
                        ui->tableWidget_1->item(row,col)->setText(rowEntries[col]);
                    }
                }
            }
        }
    }
}

//Asks if changes to be saved, then initMainTable()
void MainWindow::on_actionNew_Script_triggered()
{
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("All changes will be lost.");
    msgBox.setInformativeText("Do you want to save your changes?");
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    int ret = msgBox.exec();

    switch (ret) {
      case QMessageBox::Save:
          // Save was clicked
          on_actionSave_Script_triggered();
          initMainTable();
          break;
      case QMessageBox::Discard:
          // Don't Save was clicked
          initMainTable();
          break;
      case QMessageBox::Cancel:
          // Cancel was clicked
          break;
      default:
          // should never be reached
          break;
    }
}

//this destructor is never actually used
MainWindow::~MainWindow()
{
    delete ui;
    delete mySampleTable;
}

//Quits Program
void MainWindow::on_actionQuit_triggered()
{
    on_actionNew_Script_triggered();
    QApplication::quit();
}



//Nothing Here
void MainWindow::on_instrumentCombo_activated(const QString &arg1)
{

}

//Reveals Documentation
void MainWindow::on_actionHow_To_triggered()
{
    QString str(QDir::currentPath());
    str.append("/ScriptMax Version 1_dcumentation.pdf");
    QDesktopServices::openUrl(QUrl::fromLocalFile(str));
    //QDesktopServices::openUrl(QUrl("file:///", QUrl::TolerantMode));
}

//Shows About Box
void MainWindow::on_actionAbout_ScriptMax_triggered()
{
    QMessageBox msgBox;
    msgBox.setText("ScriptMax v1.1.\nPlease use at your own risk!");
    msgBox.exec();
}

//Clears Table
void MainWindow::on_clearTableButton_clicked()
{
    bool sure;
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("Warning");
    msgBox.setInformativeText("Are you sure you want to clear the table? All entries will be lost."\
    "to clear the script, go to File>New Script");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    switch (ret) {
       case QMessageBox::Yes:
           sure = true;
           break;
       case QMessageBox::Cancel:
           sure = false;
           break;
       default:
           // should never be reached
           break;
     }
    if (sure) initMainTable();
}



void MainWindow::closeEvent(QCloseEvent *event)
{
    if (areyousure()) {
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::areyousure()
{
    if (ui->checkBox->isChecked() || mySampleTable->sampleList.isEmpty())
        return true;
    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("Application"),
                               tr("The document has been modified.\n"
                                  "Are you sure you want to leave without saving?"),
                               QMessageBox::Yes | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Yes:
        return true;
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

//-------------------------------------------------------------------------------------------------------//
//--------------------------------SAVE STUFF-------------------------------------------------------------//
//-------------------------------------------------------------------------------------------------------//

void MainWindow::on_actionSave_GCL_file_triggered()
{
    if (ui->checkBox->isChecked())
        save(OPENGENIE);
    else{
        ui->checkBox->setChecked(true);
        ui->tabWidget->setCurrentIndex(1);
        on_checkBox_clicked();
        QMessageBox::information(this, "Save GCL file", "Please choose a file name and click 'save'.");
    }
}

void MainWindow::on_saveButton_clicked()
{
    save(OPENGENIE);
}
void MainWindow::on_PySaveButton_clicked()
{
    save(PYTHON);
}


void MainWindow::save(bool OGorPy){

    QString fileName;

    if (OGorPy == OPENGENIE)
        fileName = ui->lineEdit->text();
    else
        fileName = ui->PySaveLineEdit->text();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error","Could not save scriptfile.");
    } else {
        QTextStream stream(&file);
        if (OGorPy == OPENGENIE) stream << ui->plainTextEdit->toPlainText();
        else stream << ui->PyScriptBox->toPlainText();
        stream.flush();
        file.close();
    }

}

void MainWindow::on_checkBox_clicked()
{
    QDateTime local(QDateTime::currentDateTime());
    QString fileLoc = loadSettings().toString();
    qDebug() << "OGFileLoc" << fileLoc;
    QString fName = fileLoc + "runscript_" + local.toString("ddMMyy_hhmm") + ".gcl";

    if (ui->checkBox->isChecked()){
        ui->lineEdit->setEnabled(true);
        ui->toolButton->setEnabled(true);
        ui->saveButton->setEnabled(true);
        ui->lineEdit->setText(fName);
    } else {
        ui->lineEdit->setEnabled(false);
        ui->toolButton->setEnabled(false);
        ui->saveButton->setEnabled(false);
    }
}

void MainWindow::on_PySaveCheckBox_clicked()
{
    QDateTime local(QDateTime::currentDateTime());
    QString fileLoc = loadSettings().toString();
    QString fName = fileLoc + "runscript_" + local.toString("ddMMyy_hhmm") + ".py";

    if (ui->PySaveCheckBox->isChecked()){
        ui->PySaveLineEdit->setEnabled(true);
        ui->PyToolButton->setEnabled(true);
        ui->PySaveButton->setEnabled(true);
        ui->PySaveLineEdit->setText(fName);
    } else {
        ui->PySaveLineEdit->setEnabled(false);
        ui->PyToolButton->setEnabled(false);
        ui->PySaveButton->setEnabled(false);
    }

}

void MainWindow::on_toolButton_clicked()
{
    SaveToolButtons(OPENGENIE);
}
void MainWindow::on_PyToolButton_clicked()
{
    SaveToolButtons(PYTHON);
}

void MainWindow::SaveToolButtons(bool OGorPy){

    QDateTime local(QDateTime::currentDateTime());
    QString timestamp = local.toString("ddMMyy_hhmm");

    QString lastfileLoc = loadSettings().toString();

    QString fName;
    if (OGorPy == OPENGENIE){
       fName = QFileDialog::getSaveFileName(this,tr("Save GCL"), \
                        lastfileLoc + "runscript_" + timestamp, tr("GCL files (*.gcl)"));
        ui->lineEdit->setText(fName);
    }
    else{
        fName = QFileDialog::getSaveFileName(this,tr("Save GCL"), \
                                                lastfileLoc + "runscript_" + timestamp, tr("Python files (*.py)"));
        ui->PySaveLineEdit->setText(fName);
    }

    QString saveloc = fName.left(fName.lastIndexOf("/") + 1);
    saveSettings("lastfileloc", saveloc, "savegroup");
}


void saveSettings(const QString &key, const QVariant &value, const QString &group)
{
    QSettings settings;
    settings.beginGroup(group);
    settings.setValue(key, value);
    settings.endGroup();
}

QVariant loadSettings()
{
    QString defaultLocation = QStandardPaths::locate(QStandardPaths::DesktopLocation, QString(), QStandardPaths::LocateDirectory);
    QSettings settings;
    QVariant testloc = settings.value("lastfileloc", defaultLocation);

    return testloc;
}

//makes document with only the most important infos. Need to delete or more clearly distinguish from save().
void MainWindow::on_actionSave_Script_triggered()
{
    //the fileName is obtained in line 1247 i think. interface does not give option to name. must be automatic
    if (fileName != "") {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            // error message
        } else {
            // SAVE SAMPLE INFORMATION...
            //================================================
            QTextStream out(&file);
            out << "#SAMPLES title, translation, height, phi_offset, footprint, resolution, s3, s4\n";
            for(int i = 0; i < mySampleTable->sampleList.length(); i++){
                out << mySampleTable->sampleList[i].title << ",";
                out << QString::number(mySampleTable->sampleList[i].translation) << ",";
                out << QString::number(mySampleTable->sampleList[i].height) << ",";
                out << mySampleTable->sampleList[i].phi_offset << ",";
                out << QString::number(mySampleTable->sampleList[i].footprint) << ",";
                out << QString::number(mySampleTable->sampleList[i].resolution) << ",";
                out << QString::number(mySampleTable->sampleList[i].s3) << ",";
                out << QString::number(mySampleTable->sampleList[i].s4) << ",";
                out << QString::number(mySampleTable->sampleList[i].knauer) << "\n";
            }
            out << "#TABLE\n";
            for(int row = 0; row < ui->tableWidget_1->rowCount(); row++){
                QComboBox* box1 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,0));
                QComboBox* box2 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,1));
                if(box1 && box2){
                        out << box1->currentText() << "," << box2->currentText(); // which action and which sample etc.
                    if(box1->currentText() == "contrastchange"){
                        for(int col = 2; col < 8; col++){
                            out << "," << ui->tableWidget_1->item(row,col)->text();
                        }
                        QComboBox* box3 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,8));
                        out << "," << box3->currentText();
                        out << "\n";
                    } else if(box2->currentText().contains("Julabo Waterbath")){
                        out << "," << ui->tableWidget_1->item(row,2)->text();
                        QComboBox* box4 = qobject_cast<QComboBox*>(ui->tableWidget_1->cellWidget(row,3));
                        out << "," << box4->currentText();
                        for(int col = 4; col < 9; col++){
                            out << "," << ui->tableWidget_1->item(row,col)->text();
                        }
                        out << "\n";
                    } else {
                        for(int col = 2; col < 9; col++){
                            out << "," << ui->tableWidget_1->item(row,col)->text();
                        }
                        out << "\n";
                    }
                }
            }
        }
        file.close();
    }
    else{
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("You haven't specified a filename or directory.");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();

        switch (ret) {
          case QMessageBox::Save:
              // Save was clicked
              on_actionSave_Script_As_triggered();
              initMainTable();
              break;
          case QMessageBox::Cancel:
              // Cancel was clicked
              break;
          default:
              // should never be reached
              break;
        }
    }
}

void MainWindow::on_actionSave_Script_As_triggered()
{
    QString defaultLocation = QStandardPaths::locate(QStandardPaths::DesktopLocation, QString(), QStandardPaths::LocateDirectory);
    fileName = QFileDialog::getSaveFileName(this,tr("Save Script As..."), \
                                           defaultLocation, tr("Script files (*.scp)"));
    on_actionSave_Script_triggered();
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//==================================================SAVE STUFF OVER=====================================================================================================//
//======================================================================================================================================================================//
//======================================================================================================================================================================/


void MainWindow::on_PythonButton_clicked()
{
    pyhighlighter = new KickPythonSyntaxHighlighter(ui->PyScriptBox->document());
    writeBackbone();
}


void MainWindow::on_OGButton_clicked()
{
    OGhighlighter = new Highlighter(ui->plainTextEdit->document());
    writeBackbone();
}


void MainWindow::ProgressBar(int secs, int row){


    bar = new QProgressBar();
    bar->setMinimumSize(73, ui->tableWidget_1->rowHeight(1));//need this to fill box
    ui->tableWidget_1->setCellWidget(row,10,bar);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateProgBar(row)));
    timer->start(secs*10);

}

void MainWindow::updateProgBar(int row){

    if(counter <= 100)
    {
        counter++;
        bar->setValue(counter);
    }
    else
    {
        timer->stop();
        delete timer;
        ui->tableWidget_1->removeCellWidget(row,10);

        ui->tableWidget_1->setIconSize(QSize(90,ui->tableWidget_1->rowHeight(0)));
        QTableWidgetItem *icon_item = new QTableWidgetItem;

        icon_item->setIcon(QIcon(":/tick.png"));
        ui->tableWidget_1->setItem(row, 10, icon_item);

    }

}




