// TODO: Finalize M2I handling. What exactly is the point of that FS, is it to handle 8.3 filenames to cbm 16 byte lengths?
// TODO: Finalize Native FS routines.
// TODO: Handle all data channel stuff. TALK, UNTALK, and so on.
// TODO: Parse date and time on connection at arduino side.
// TODO: T64 / D64 formats should read out entire disk into memory for caching (network performance).
// TODO: Check the port name to use on the PI in the port constructor / setPortName(QLatin1String("/dev/ttyS0"));

#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include <QString>
#include <QFileDialog>
#include <QTextStream>
#include <QDate>

const QString OkString = "OK>";
const QColor logLevelColors[] = { QColor(Qt::red), QColor("orange"), QColor(Qt::blue), QColor(Qt::darkGreen) };

QStringList LOG_LEVELS = (QStringList()
													<< QObject::tr("error  ")
													<< QObject::tr("warning")
													<< QObject::tr("info   ")
													<< QObject::tr("success"));


MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow), m_port("COM1"), m_isConnected(false), m_iface(m_port)
{
	ui->setupUi(this);
//	m_port = new QextSerialPort("COM1"); // make RPI serial port configurable and persistent from GUI.
	connect(&m_port, SIGNAL(readyRead()), this, SLOT(onDataAvailable()));
} // MainWindow


void MainWindow::onDataAvailable()
{
//	bool wasEmpty = m_pendingBuffer.isEmpty();
	m_pendingBuffer.append(m_port.readAll());
	if(!m_isConnected) {
		if(m_pendingBuffer.contains("CONNECT")) {
			m_port.write((OkString + QDate::currentDate().toString("yyyy-MM-dd") +
										 QTime::currentTime().toString(" hh:mm:ss:zzz") + '\r').toLatin1().data());
			m_isConnected = true;
			// client is supposed to send it's facilities each start.
			m_clientFacilities.clear();
		}
	}
	bool hasDataToProcess = true;
	while(hasDataToProcess) {
		QString cmdString(m_pendingBuffer);
		int crIndex =	cmdString.indexOf('\r');

		switch(cmdString.at(0).toLatin1()) {
			case 'R': // read single byte from current file system mode, note that this command needs no termination, it needs to be short.
				m_pendingBuffer.remove(0, 1);
				hasDataToProcess = !m_pendingBuffer.isEmpty();
				break;

			case 'S': // set read / write size in number of bytes.
				break;

			case 'W': // write single character to file in current file system mode.
				break;

			case '!': // register facility string.
				if(-1 == crIndex)
					hasDataToProcess = false; // escape from here, command is incomplete.
				else {
					processAddNewFacility(cmdString.left(crIndex));
					m_pendingBuffer.remove(0, crIndex + 1);
				}
				break;

			case 'D': // debug output.
				if(-1 == crIndex)
					hasDataToProcess = false; // escape from here, command is incomplete.
				else {
					processDebug(cmdString.left(crIndex));
					m_pendingBuffer.remove(0, crIndex + 1);
				}
				break;

			case 'M': // set mode and include command string.
				break;

			case 'O': // open command
				if(-1 == crIndex)
					hasDataToProcess = false; // escape from here, command is incomplete.
				else
					m_iface.processOpenCommand(cmdString.mid(1, crIndex - 1));
				break;

		}
	} // while(hasDataToProcess);
} // onDataAvailable


void MainWindow::processAddNewFacility(const QString& str)
{
	m_clientFacilities[str.at(1)] = str.right(2);
} // processAddNewFacility


void MainWindow::processDebug(const QString& str)
{
	LogLevelE level = info;
	switch(str[1].toUpper().toLatin1()) {
		case 'S':
			level = success;
			break;
		case 'I':
			level = success;
			break;
		case 'W':
			level = warning;
			break;
		case 'E':
			level = error;
			break;
	}

	Log(m_clientFacilities.value(str[2], "GENERAL"), str.right(3), level);
} // processDebug


MainWindow::~MainWindow()
{
	delete ui;
} // dtor


////////////////////////////////////////////////////////////////////////////////
// Log related implementation
////////////////////////////////////////////////////////////////////////////////
void MainWindow::on_clearLog_clicked()
{
	ui->log->clear();
	ui->saveHtml->setEnabled(false);
	ui->saveLog->setEnabled(false);
	ui->clearLog->setEnabled(false);
	ui->pauseLog->setEnabled(false);
} // on_m_clearLog_clicked


void MainWindow::on_saveLog_clicked()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Log"), QString(), tr("Text Files (*.log *.txt)"));
	if(!fileName.isEmpty()) {
		QFile file(fileName);
		if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream out(&file);
		out << ui->log->toPlainText();
		file.close();
	}
} // on_saveLog_clicked


void MainWindow::on_saveHtml_clicked()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Log as HTML"), QString(), tr("Html Files (*.html *.htm)"));
	if(!fileName.isEmpty()) {
		QFile file(fileName);
		if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream out(&file);
		out << ui->log->toHtml();
		file.close();
	}
} // on_saveHtml_clicked


void MainWindow::Log(const QString& facility, const QString& message, LogLevelE level)
{
	if(!ui->pauseLog->isChecked()) {
		QTextCursor cursor = ui->log->textCursor();
		cursor.movePosition(QTextCursor::End);
		ui->log->setTextCursor(cursor);

		ui->log->setTextColor(Qt::darkGray);

		ui->log->insertPlainText(QDate::currentDate().toString("yyyy-MM-dd") +
											QTime::currentTime().toString(" hh:mm:ss:zzz"));
		ui->log->setFontWeight(QFont::Bold);
		ui->log->setTextColor(logLevelColors[level]);
		// The logging levels are: [E]RROR [W]ARNING [I]NFORMATION [S]UCCESS.
		ui->log->insertPlainText(" " + QString("EWIS")[level] + " " + facility + " ");
		ui->log->setFontWeight(QFont::Normal);
		ui->log->setTextColor(Qt::darkBlue);
		ui->log->insertPlainText(message + "\n");

		if(ui->log->toPlainText().length() and !ui->saveHtml->isEnabled()) {
			ui->saveHtml->setEnabled(true);
			ui->saveLog->setEnabled(true);
			ui->clearLog->setEnabled(true);
			ui->pauseLog->setEnabled(true);
		}
	}
} // Log


void MainWindow::on_pauseLog_toggled(bool checked)
{
	ui->pauseLog->setText(checked ? tr("Resume") : tr("Pause"));
} // on_m_pauseLog_toggled