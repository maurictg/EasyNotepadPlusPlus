#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <etab.h>
#include <QMessageBox>
#include <QTextCharFormat>
#include <QTime>
#include <QTimer>
#include <QFileInfo>
#include <QFileDialog>
#include <QStandardPaths>
#include <QCloseEvent>
#include <QTabWidget>
#include <QList>
#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <iostream>
#include <QJsonObject>
#include <QJsonArray>
#include <qjsondocument.h>

MainWindow::MainWindow(QStringList* params, QJsonObject* json, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QFont font("Consolas", 12);
    ui->tabs->setFont(font);

    font.setPointSize(10);

    //Initialize status bar
    this->lblClock = new QLabel("Hi!");
    lblClock->setAlignment(Qt::AlignLeft);
    lblClock->setFont(font);

    this->lblStatus = new QLabel("ln: 0 col: 0 ");
    lblStatus->setAlignment(Qt::AlignRight);
    lblStatus->setFont(font);

    statusBar()->addWidget(lblClock, 1);
    statusBar()->addWidget(lblStatus, 1);
    statusBar()->setFont(font);

    updateTime();
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(5000);

    setAcceptDrops(true);
    this->donotload = false;

    tempfile = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + "easynotepad.json");

    this->params = params;
    this->settings = json;
}

MainWindow::~MainWindow()
{
    if(params != nullptr) {
        params->clear();
    }
    delete ui;
}

/*
 * Event handlers
 */

//Updates time label in statusBar
void MainWindow::updateTime() {
    lblClock->setText("  \U0001F550 "+QTime::currentTime().toString("HH:mm"));
}


//Bold, italic, underline, strikeout events
void MainWindow::on_actionBold_triggered()
{
    QTextCharFormat fmt;
    fmt.setFontWeight(ui->actionBold->isChecked() ? QFont::Bold : QFont::Normal);
    setFontOnSelected(fmt);
}

void MainWindow::on_actionItalic_triggered()
{
    QTextCharFormat fmt;
    fmt.setFontItalic(ui->actionItalic->isChecked());
    setFontOnSelected(fmt);
}

void MainWindow::on_actionUnderline_triggered()
{
    QTextCharFormat fmt;
    fmt.setFontUnderline(ui->actionUnderline->isChecked());
    setFontOnSelected(fmt);
}

void MainWindow::on_actionStrikeout_triggered()
{
    QTextCharFormat fmt;
    fmt.setFontStrikeOut(ui->actionStrikeout->isChecked());
    setFontOnSelected(fmt);
}

//I/O actions
void MainWindow::on_actionOpen_triggered()
{
    QFileDialog fileDialog(this, tr("Open File(s)..."));
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setMimeTypeFilters(QStringList()
#if QT_CONFIG(texthtmlparser)
                                  << "text/html"
#endif
#if QT_CONFIG(textmarkdownreader)

                                  << "text/markdown"
#endif
                                  << "text/plain");
    if (fileDialog.exec() != QDialog::Accepted)
        return;

    for(QString file : fileDialog.selectedFiles()){
        openTab(file);
    }
}

void MainWindow::on_actionSave_as_triggered()
{
    QFileDialog fileDialog(this, tr("Save as..."));
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    QStringList mimeTypes;
    mimeTypes
              << "text/html"
             #if QT_CONFIG(textodfwriter)
                           << "application/vnd.oasis.opendocument.text"
             #endif
             #if QT_CONFIG(textmarkdownwriter)
                           << "text/markdown"
             #endif
             << "text/plain";
    fileDialog.setMimeTypeFilters(mimeTypes);
#if QT_CONFIG(textodfwriter)
    fileDialog.setDefaultSuffix("odt");
#endif

    if (fileDialog.exec() != QDialog::Accepted)
        return;
    const QString filename = fileDialog.selectedFiles().first();

    //Get selected tab
    ETab *selected = ui->tabs->findChild<ETab *>(ui->tabs->currentWidget()->objectName());
    if(selected == NULL){
        std::cerr << "Error: selected tab is NULL" << std::endl;
        return;
    }

    selected->setFileName(filename);
    QFileInfo info(filename);
    QFile f(filename);
    f.open(QIODevice::ReadWrite); //This creates the file
    ui->tabs->setTabText(ui->tabs->currentIndex(), info.fileName());
    changeTab(ACTION::SAVEAS);
}

//Save file
void MainWindow::on_actionSave_triggered() { changeTab(ACTION::SAVE); }

//Delete file
void MainWindow::on_actionDelete_file_triggered()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete file?", "Are you sure you want to delete this file?",
                                    QMessageBox::Yes|QMessageBox::Cancel);
    if(reply == QMessageBox::Yes){
        changeTab(ACTION::DELETE);
    }
}

//Event that is triggered when the window loads
void MainWindow::showEvent(QShowEvent *event){
    if(donotload)
        return;

    QWidget::showEvent(event);

    //Try to load temp file
    this->index = 0;

    if(params->size() > 0) {
        for(int i = 0; i < params->size(); i++) {
            QString name = params->at(i);
            if(name != "#Close") {
                openTab(name);
            }
        }
        delete params;
    } else {
        this->loadTempFile();
    }

    statusBar()->clearMessage();
    this->donotload = true;
}

//Event thats triggered when user quits
void MainWindow::closeEvent(QCloseEvent *event){
    on_action_Exit_triggered();
    event->ignore();
}

//Other menu items
void MainWindow::on_action_New_triggered() { this->openTab("New File"); }
void MainWindow::on_action_Close_triggered() { changeTab(ACTION::CLOSE); }
void MainWindow::on_actionBigger_triggered() { changeTab(ACTION::CHANGEFONTSIZE, true); }
void MainWindow::on_actionSmaller_triggered() { changeTab(ACTION::CHANGEFONTSIZE, false); }
void MainWindow::on_actionColor_triggered() { changeTab(ACTION::CHANGECOLOR);}
void MainWindow::on_actionFont_family_triggered() { changeTab(ACTION::CHANGEFONT); }
void MainWindow::on_actionAutosave_triggered() { changeTab(ACTION::SETAUTOSAVE); }
void MainWindow::on_actionForce_Quit_triggered() { exit(0); }

void MainWindow::on_actionStandard_triggered() { changeTab(ACTION::SETHNORMAL); }
void MainWindow::on_actionHeading_1_triggered() { changeTab(ACTION::SETH1); }
void MainWindow::on_actionHeading_2_triggered() { changeTab(ACTION::SETH2); }
void MainWindow::on_actionHeading_3_triggered() { changeTab(ACTION::SETH3); }
void MainWindow::on_actionHeading_4_triggered() { changeTab(ACTION::SETH4); }
void MainWindow::on_actionHeading_5_triggered() { changeTab(ACTION::SETH5); }
void MainWindow::on_actionHeading_6_triggered() { changeTab(ACTION::SETH6); }

void MainWindow::on_actionCircle_triggered() { changeTab(ACTION::LISTCIRCLE); }
void MainWindow::on_actionSquare_triggered() { changeTab(ACTION::LISTSQUARE); }
void MainWindow::on_actionDisc_triggered() { changeTab(ACTION::LISTDISK); }
void MainWindow::on_actionAlpha_lower_triggered() { changeTab(ACTION::LISTALPHALOWER); }
void MainWindow::on_actionAlpha_upper_triggered() { changeTab(ACTION::LISTALPHAUPPER); }
void MainWindow::on_actionRoman_lower_triggered() { changeTab(ACTION::LISTROMANLOWER); }
void MainWindow::on_actionRoman_upper_triggered() { changeTab(ACTION::LISTROMANUPPER); }
void MainWindow::on_actionStandard_numeric_triggered() { changeTab(ACTION::LISTDECIMAL); }
void MainWindow::on_actionCheckbox_triggered() { changeTab(ACTION::LISTUNCHECKED); }
void MainWindow::on_actionCheckbox_checked_triggered() { changeTab(ACTION::LISTCHECKED); }

void MainWindow::on_actionLeft_triggered() { changeTab(ACTION::ALIGNLEFT); }
void MainWindow::on_actionRight_triggered() { changeTab(ACTION::ALIGNRIGHT); }
void MainWindow::on_actionCenter_triggered() { changeTab(ACTION::ALIGNCENTER); }
void MainWindow::on_actionJustify_triggered() { changeTab(ACTION::ALIGNJUSTIFY); }

void MainWindow::on_actionUse_default_triggered() { setTheme(THEME::DEFAULT, 1); }
void MainWindow::on_actionUse_light_theme_triggered() { setTheme(THEME::LIGHT, 1); }
void MainWindow::on_actionUse_dark_theme_triggered() { setTheme(THEME::DARK, 1); }
void MainWindow::on_actionUse_blue_theme_triggered() { setTheme(THEME::BLUE, 1); }

void MainWindow::on_actionHyperlink_triggered() { changeTab(ACTION::CREATELINK); }

void MainWindow::on_actionRemeber_opened_files_triggered() {}


//Exit application
void MainWindow::on_action_Exit_triggered()
{
    saveTempFile();
    on_actionClose_all_triggered();

    std::cout << "Bye!" << std::endl;
    QApplication::exit(0);
}

//Enable/disable topmost
void MainWindow::on_actionStay_topmost_triggered()
{
    //Maybe other code on windows: https://stackoverflow.com/a/2860768
    if(ui->actionStay_topmost->isChecked()){
        this->setWindowFlags(this->windowFlags() | Qt::WindowStaysOnTopHint);
        std::cout << "Change topmost" << std::endl;
    } else{
        this->setWindowFlags(this->windowFlags() ^ Qt::WindowStaysOnTopHint);
        std::cout << "Change not topmost" << std::endl;
    }
    this->show();
}

//Close all tabs
void MainWindow::on_actionClose_all_triggered()
{
    int cnt = ui->tabs->count();
    while(ui->tabs->count()>0){
        changeTab(ACTION::CLOSE);
    }

    updateMessage(QString("Closed %1 files").arg(cnt));
}


//Drag and drop
void MainWindow::dragEnterEvent(QDragEnterEvent *event){
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event){
    foreach (const QUrl &url, event->mimeData()->urls()) {
        openTab(url.toLocalFile());
    }
}

/*
 * Logic
 */

//Load temp file if exists
void MainWindow::loadTempFile(){    
    bool restoreAny = false;

    if(settings != nullptr) {
        QJsonObject json = *settings;

        //Saved files
        if(json.contains("files") && json["files"].isArray()) {
            QJsonArray files = json["files"].toArray();
            if(files.count() > 0){
                for (int i = 0; i < files.size(); i++) {
                    openTab(files[i].toString());
                }

                restoreAny = true;
                ui->actionRemeber_opened_files->setChecked(true);
            }
        }

        //Screen resolution and other settings. Maybe object "settings"
        if(json.contains("resolution") && json["resolution"].isArray()) {
            QJsonArray arr = json["resolution"].toArray();
            if(arr.size() == 2) {
                int width = arr[0].toInt();
                int heigth = arr[1].toInt();
                //This is how i resize window without struggles :D
                MainWindow::setFixedSize(width, heigth);
                MainWindow::setMinimumSize(0,0);
                MainWindow::setMaximumSize(16777215,16777215);
            }
        }

        //Open editors (temp files/quick notes)
        if(json.contains("editors") && json["editors"].isArray()) {
            QJsonArray arr = json["editors"].toArray();
            if(arr.size() > 0) {
                for(int i = 0; i < arr.size(); i++) {
                    if(arr[i].isObject()) {
                        QJsonObject o = arr[i].toObject();
                        openTab(QString("Quick note #%1").arg(i+1));
                        ETab *selected = ui->tabs->findChild<ETab *>(ui->tabs->currentWidget()->objectName());
                        selected->setContent(o["content"].toString());
                    }
                }

                restoreAny = true;
                ui->actionRemember_quick_notes->setChecked(true);
            }
        }

        //Get theme
        if(json.contains("theme")) {
            setTheme((THEME)json["theme"].toInt());
        }
    }

    //If nothing is opened: open new tab
    if(!restoreAny) {
        this->openTab("New file");
    }
}

//Write temp file to disk
void MainWindow::saveTempFile(){
    QFile f(tempfile);
    if(!f.open(QIODevice::WriteOnly)){
        std::cerr << "ERROR: Failed to save remembered files" << std::endl;
        return;
    }

    QJsonArray files; //Files to be reloaded from disk
    QJsonArray editors; //Open editors that will be saved

    QList<ETab*> tabs = ui->tabs->findChildren<ETab*>() ;
    for (ETab* t : tabs) {
        if(t->fileExists()) {
            if(ui->actionRemeber_opened_files->isChecked()) {
                files.append(t->getFileName());
            }
        } else {
            if(ui->actionRemember_quick_notes->isChecked() && t->getFileName() != "#About") {
                QJsonObject editor;
                editor["content"] = t->getContent();

                if(t->hasChanges()) {
                    editors.append(editor);
                }

                t->setContent("", false); //Trick to not save file
                delete t; //Close tab
            }
        }
    }

    QJsonArray resolution;
    if(MainWindow::isMaximized()) {
        resolution.append(MainWindow::normalGeometry().width());
        resolution.append(MainWindow::normalGeometry().height());
    } else {
        resolution.append(MainWindow::width());
        resolution.append(MainWindow::height());
    }

    QJsonObject object;
    object["files"] = files;
    object["resolution"] = resolution;
    object["editors"] = editors;
    object["theme"] = (int)this->theme;

    QJsonDocument doc(object);
    QString res = doc.toJson();
    f.write(res.toUtf8());
    f.close();
}

//Set autosave checked/unchecked
void MainWindow::updateAutoSave(bool checked){
    ui->actionAutosave->setChecked(checked);
}

//Set font on selected tab
void MainWindow::setFontOnSelected(const QTextCharFormat &format){
    //Get selected tab
    ETab *selected = ui->tabs->findChild<ETab *>(ui->tabs->currentWidget()->objectName());
    if(selected == NULL){
        std::cerr << "ERROR: selected tab is NULL" << std::endl;
        return;
    }

    selected->setFontFormat(format);
}

//Open new tab by file name
void MainWindow::openTab(QString file){
    //Add tab to tabs
    ETab *tab = new ETab(this);
    int tabCount = ui->tabs->count();
    tab->setObjectName(QString("tab-%1").arg(index++));
    tab->setFileName(file);

    QFileInfo fi(file);
    QString title = fi.fileName();

    if(fi.exists()){
        tab->openFile();
    }

    ui->tabs->addTab(tab, title);
    ui->tabs->setCurrentIndex(tabCount);
    tab->focus();
    updateActions();
    updateMessage(" \U0001F5CE "+title+" opened!");
}

//Disable/enable actions
void MainWindow::updateActions() {
    bool enabled = (ui->tabs->count()!=0);

    ui->action_Close->setEnabled(enabled);
    ui->actionSave->setEnabled(enabled);
    ui->actionSave_as->setEnabled(enabled);
    ui->actionDelete_file->setEnabled(enabled);
    ui->actionClose_all->setEnabled(enabled);
    ui->actionAutosave->setEnabled(enabled);
    toggleMenu(ui->menuEdit, !enabled);
}

//Recursive function to toggle menu's
void MainWindow::toggleMenu(QMenu *menu, bool disable){
    for(QAction *action : menu->actions()){
        if(!action->isSeparator()){
            action->setEnabled(!disable);
            if(action->menu()){
                toggleMenu(action->menu(), disable);
            }
        }
    }
}

//Update bold/italic/underline/strikeout actions when selecting text. Triggered from ETab logic
void MainWindow::updateActions(const QTextCharFormat &format){
    ui->actionBold->setChecked(format.font().bold());
    ui->actionUnderline->setChecked(format.font().underline());
    ui->actionItalic->setChecked(format.font().italic());
    ui->actionStrikeout->setChecked(format.font().strikeOut());
}

//Update status label. Triggered from ETab logic
void MainWindow::updateStatusLabel(int line, int col){
    lblStatus->setText(QString("ln: %1 col: %2 ").arg(line).arg(col));
}

//Show message in statusBar
void MainWindow::updateMessage(QString message){
    ui->statusbar->showMessage(message, 3000);
}


//Execute actions on selected tab
void MainWindow::changeTab(ACTION action, int argument){
    ETab *selected = ui->tabs->findChild<ETab *>(ui->tabs->currentWidget()->objectName());
    if(selected == NULL){
        std::cerr << "ERROR: selected tab is NULL" << std::endl;
        return;
    }

    QFileInfo info(selected->getFileName());

    switch (action) {
        case ACTION::CHANGEFONT:
            selected->changeFont();
        break;
        case ACTION::CHANGECOLOR:
            selected->changeColor();
        break;
        case ACTION::CHANGEFONTSIZE:
            selected->changeFontSize(argument);
        break;
        case ACTION::SAVE:
        {
            if(!info.exists())
                on_actionSave_as_triggered();
            selected->saveFile();
        }
        break;
        case ACTION::SAVEAS:
        {
            selected->saveFile(true);
        }
        break;
        case ACTION::DELETE:
        {
            QFile file(selected->getFileName());
            changeTab(ACTION::CLOSE);
            if(!info.exists())
                return;
            file.remove();
            updateMessage("Deleted "+info.fileName());
        }
        break;
        case ACTION::CLOSE:
        {
            //Close and current tab
            if(!info.exists() && selected->hasChanges())
            {
                QMessageBox::StandardButton res = QMessageBox::question(this, "Save file?", QString("Do you want to save %1?").arg(info.fileName()), QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);
                if(res == QMessageBox::Save){
                    on_actionSave_as_triggered();
                } else if(res == QMessageBox::Cancel) {
                    return;
                }
            }
            else
                selected->saveFile();

            delete selected;
            //ui->tabs->removeTab(ui->tabs->currentIndex());
            updateActions();
            updateMessage(" \U0001F5CE "+info.fileName()+" closed!");
        }
        break;
        case ACTION::SETAUTOSAVE:
            selected->setAutoSave(ui->actionAutosave->isChecked());
        break;
        case ACTION::SETHNORMAL: case ACTION::SETH1: case ACTION::SETH2: case ACTION::SETH3: case ACTION::SETH4: case ACTION::SETH5: case ACTION::SETH6:
            selected->setStyle((action == ACTION::SETHNORMAL) ? 0 : (action - ACTION::SETHNORMAL)+10);
        break;
        case ACTION::LISTDISK: case ACTION::LISTCIRCLE: case ACTION::LISTSQUARE: case ACTION::LISTCHECKED: case ACTION::LISTDECIMAL: case ACTION::LISTUNCHECKED: case ACTION::LISTALPHALOWER: case ACTION::LISTALPHAUPPER: case ACTION::LISTROMANLOWER: case ACTION::LISTROMANUPPER:
            selected->setStyle((action - ACTION::LISTDISK) + 1);
        break;
        case ACTION::ALIGNLEFT: case ACTION::ALIGNRIGHT: case ACTION::ALIGNCENTER: case ACTION::ALIGNJUSTIFY:
            selected->setAlign((action - ACTION::ALIGNLEFT));
        break;
        case ACTION::CREATELINK:
            selected->createLink();
        break;
    }
}

//Set application theme
void MainWindow::setTheme(THEME theme, bool showMessage) {
    this->theme = theme;
    for(QAction *action : ui->menuTheme->actions()){
        action->setChecked(false);
    }

    switch (theme) {
        case LIGHT:
        ui->actionUse_light_theme->setChecked(true);
            break;
        case DARK:
        ui->actionUse_dark_theme->setChecked(true);
            break;
        case BLUE:
        ui->actionUse_blue_theme->setChecked(true);
            break;
        default:
        ui->actionUse_default->setChecked(true);
            break;
    }

    if(showMessage) {
        QMessageBox::information(this, "Theme change", "Theme change will be applied after application restart.");
    }
}

void MainWindow::on_actionAbout_triggered()
{
    openTab("#About");
    QString hardCodedAboutPage = "<h1>EasyNotepad++</h1><p>EasyNotepad++ is an richtext editor for Windows and Linux. It supports HTML, Markdown, and plain text files. It is also able to export a file to an ODT-file. It is made with QT and C++</p><h1>Licence</h1><p>EasyNotepad is licenced under the MIT-licence.</p><br/><h4>&rarr;&nbsp;More info, see <a href=\"https://github.com/maurictg/EasyNotepadPlusPlus\">https://github.com/maurictg/EasyNotepadPlusPlus</a></h4>";
    ETab *selected = ui->tabs->findChild<ETab *>(ui->tabs->currentWidget()->objectName());
    selected->setContent(hardCodedAboutPage, false);
}




