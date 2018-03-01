#ifndef DLGTRANSFERFILE_H
#define DLGTRANSFERFILE_H

#include <QDialog>
#include <QDragEnterEvent>
#include <QDebug>
#include <QListWidget>
#include <QListWidget>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDebug>
#include "P2PController.h"
#include <QFileSystemModel>
#include <QTableWidget>
#include <deque>
#include <QMovie>


namespace Ui {
  class DlgTransferFile;
}

class FileThreadDownloader : public QObject{
  Q_OBJECT
  QString remote_user, remote_ip, remote_port, key;
  QString file_path, file_desination;

public:
  FileThreadDownloader(QObject *parent = nullptr) : QObject (parent)
  {

  }
  void init (const QString &remote_user,
             const QString &remote_ip,
             const QString &remote_port,
             const QString &file_path,
             const QString &file_destination,
             const QString &key) {
    this->remote_user = remote_user;
    this->remote_ip = remote_ip;
    this->remote_port = remote_port;
    this->file_path = file_path;
    this->file_desination = file_destination;
    this->key = key;
  }

  void startWork() {
    QThread* thread = new QThread();
    connect(thread, &QThread::started,
            this, &FileThreadDownloader::execute_remote_command);
    connect(this, &FileThreadDownloader::outputReceived,
            thread, &QThread::quit);
    connect(thread, &QThread::finished,
            this, &FileThreadDownloader::deleteLater);
    connect(thread, &QThread::finished,
            thread, &QThread::deleteLater);
    this->moveToThread(thread);
    thread->start();
  }


  void execute_remote_command() {
    //QStringList output;
    QFutureWatcher< std::pair<system_call_wrapper_error_t, QStringList> > *watcher
        = new QFutureWatcher<std::pair<system_call_wrapper_error_t, QStringList>>(this);

    QFuture< std::pair<system_call_wrapper_error_t, QStringList> >  res =
        QtConcurrent::run(CSystemCallWrapper::download_file,
                          remote_user, remote_ip, std::make_pair(remote_port, key), file_desination, file_path);
    watcher->setFuture(res);
    connect(watcher, &QFutureWatcher<std::pair<system_call_wrapper_error_t, QStringList> >::finished, [this, res](){
      emit this->outputReceived(res.result().first, res.result().second);
    });
  }

signals:
  void outputReceived(system_call_wrapper_error_t res, QStringList output);
};


class FileThreadUploader : public QObject{
  Q_OBJECT
  QString remote_user, remote_ip, remote_port, key;
  QString file_path, file_desination;

public:
  FileThreadUploader(QObject *parent = nullptr) : QObject (parent)
  {

  }
  void init (const QString &remote_user,
             const QString &remote_ip,
             const QString &remote_port,
             const QString &file_path,
             const QString &file_destination,
             const QString &key) {
    this->remote_user = remote_user;
    this->remote_ip = remote_ip;
    this->remote_port = remote_port;
    this->file_path = file_path;
    this->file_desination = file_destination;
    this->key = key;
  }

  void startWork() {
    QThread* thread = new QThread();
    connect(thread, &QThread::started,
            this, &FileThreadUploader::execute_remote_command);
    connect(this, &FileThreadUploader::outputReceived,
            thread, &QThread::quit);
    connect(thread, &QThread::finished,
            this, &FileThreadUploader::deleteLater);
    connect(thread, &QThread::finished,
            thread, &QThread::deleteLater);
    this->moveToThread(thread);
    thread->start();
  }


  void execute_remote_command() {
    //QStringList output;
    QFutureWatcher<std::pair<system_call_wrapper_error_t, QStringList> > *watcher
        = new QFutureWatcher<std::pair<system_call_wrapper_error_t, QStringList> >(this);

    QFuture<std::pair<system_call_wrapper_error_t, QStringList> >  res =
        QtConcurrent::run(CSystemCallWrapper::upload_file,
                          remote_user, remote_ip, std::make_pair(remote_port, key), file_desination, file_path);
    watcher->setFuture(res);
    connect(watcher, &QFutureWatcher<system_call_wrapper_error_t>::finished, [this, res](){
      emit this->outputReceived(res.result().first, res.result().second);
    });
  }

signals:
  void outputReceived(system_call_wrapper_error_t res, QStringList output);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////

class RemoteCommandExecutor : public QObject{
  Q_OBJECT
  QString remote_user, remote_ip, remote_port;
  QString command, key;

public:
  RemoteCommandExecutor(QObject *parent = nullptr) : QObject (parent)
  {

  }
  void init (const QString &remote_user,
             const QString &remote_ip,
             const QString &remote_port,
             const QString &command,
             const QString &key) {
    this->remote_user = remote_user;
    this->remote_ip = remote_ip;
    this->remote_port = remote_port;
    this->command = command;
    this->key = key;
  }

  void startWork() {
    QThread* thread = new QThread();
    connect(thread, &QThread::started,
            this, &RemoteCommandExecutor::execute_remote_command);
    connect(this, &RemoteCommandExecutor::outputReceived,
            thread, &QThread::quit);
    connect(thread, &QThread::finished,
            this, &RemoteCommandExecutor::deleteLater);
    connect(thread, &QThread::finished,
            thread, &QThread::deleteLater);
    this->moveToThread(thread);
    thread->start();
  }


  void execute_remote_command() {
    //QStringList output;
    QFutureWatcher<std::pair<system_call_wrapper_error_t, QStringList>> *watcher
        = new QFutureWatcher<std::pair<system_call_wrapper_error_t,QStringList> >(this);

    QFuture<std::pair<system_call_wrapper_error_t, QStringList> >  res =
        QtConcurrent::run(CSystemCallWrapper::send_command,
                          remote_user, remote_ip, remote_port, command, key);
    watcher->setFuture(res);
    connect(watcher, &QFutureWatcher<std::pair<system_call_wrapper_error_t, QStringList>>::finished, [this, res](){
      emit this->outputReceived(res.result().first, res.result().second);
    });
  }

signals:
  void outputReceived(system_call_wrapper_error_t res, const QStringList &output);
};


enum FILE_TYPE {
  FILE_TYPE_SIMPLE = 0,
  FILE_TYPE_DIRECTORY,
  UNKNOWN
};

enum TRANSFER_FILE_STATUS{
  FILE_WITHOUT_OPERATION = 0,
  // UPLOAD STATUSES
  FILE_TO_UPLOAD,
  FIlE_FAILED_TO_UPLOAD,
  FILE_FINISHED_UPLOAD,
  // DOWNLOAD STATUSES
  FILE_TO_DOWNLOAD,
  FILE_FINISHED_DOWNLOAD,
  FILE_FAILED_TO_DOWNLOAD,
};

enum MACHINE_TYPE{
  MACHINE_UNKNOWN = 0,
  LOCAL_MACHINE,
  REMOTE_MACHINE
};


class OneFile
{
  QString m_fileName;
  QString m_filePath;
  QDateTime m_created;
  quint64 m_fileSize;
  FILE_TYPE m_fileType;

public:
  OneFile(const QString &fileName,
          const QString &filePath,
          const QDateTime &created,
          quint64 fileSize,
          FILE_TYPE fileType)
  {
    m_fileName = fileName;
    m_filePath = filePath;
    m_created = created;
    m_fileSize = fileSize;
    m_fileType = fileType;
  }
  OneFile() {

  }

  const QString &fileName() const {
    return m_fileName;
  }
  const QString &filePath() const {
    return m_filePath;
  }

  const QDateTime &created() const {
    return m_created;
  }

  FILE_TYPE fileType() const {
    return m_fileType;
  }

  quint64 fileSize() const {
    return m_fileSize;
  }
};

class FileToTransfer {

private:
  OneFile m_fileInfo;
  QString m_destinationPath;
  MACHINE_TYPE m_sourceMachineType;
  TRANSFER_FILE_STATUS m_fileStatus;

public:
  const OneFile &fileInfo () const {
    return m_fileInfo;
  }
  const QString &destinationPath() const {
    return m_destinationPath;
  }
  MACHINE_TYPE sourceMachineType () const {
    return m_sourceMachineType;
  }
  TRANSFER_FILE_STATUS currentFileStatus () const {
    return m_fileStatus;
  }
  void setFileInfo(const OneFile &fileInfo) {
    m_fileInfo = fileInfo;
  }

  void setDesinationPath(const QString destinationPath) {
    m_destinationPath = destinationPath;
  }

  void setSourceMachineType(MACHINE_TYPE sourceMachineType) {
    m_sourceMachineType = sourceMachineType;
  }

  void setTransferFileStatus(TRANSFER_FILE_STATUS fileStatus) {
    m_fileStatus = fileStatus;
  }

};

#include "NotificationObserver.h"

class FileSystemTableWidget : public QTableWidget
{
  Q_OBJECT
public:
  FileSystemTableWidget(QWidget *parent) : QTableWidget(parent) {}
protected:
  void dragMoveEvent(QDragMoveEvent *e) {
    if (e->source() != this) {
      e->accept();
    } else {
      e->ignore();
    }
  }

  QStringList mimeTypes() const {
    return QStringList("text/plain");
  }
  void dropEvent(QDropEvent *e) {
    UNUSED_ARG (e);
    // int row = columnAt(e->pos().y());
    // int column = columnAt(e->pos().x());
    emit something_is_dropped();
  }
signals:
  void something_is_dropped();
};

class DropFileTableWidget : public QTableWidget
{
  Q_OBJECT
public:
  explicit DropFileTableWidget(QWidget *parent = Q_NULLPTR) :
    QTableWidget(parent) {
    setAcceptDrops(true);
  }

private:
  void dragEnterEvent(QDragEnterEvent *event) override {
    setBackgroundRole(QPalette::Highlight);
    if (event->mimeData()->hasUrls()) {
      event->acceptProposedAction();
    }
  }

  void dragMoveEvent(QDragMoveEvent *event) override {
    event->acceptProposedAction();
  }

  void dropEvent(QDropEvent *event) override {
    foreach (const QUrl &url, event->mimeData()->urls()) {
      QString filePath = url.toLocalFile();
      emit file_was_dropped(filePath);
    }
  }

signals:
  void file_was_dropped(const QString &file_path);
};


class DlgTransferFile : public QDialog
{
  Q_OBJECT

public:
  explicit DlgTransferFile(QWidget *parent = 0);
  ~DlgTransferFile();
  void addSSHKey(const QString &key);
  void addIPPort(const QString &ip, const QString &port);
  void addUser(const QString &user);
  void parse_remote_file(const QString &file_info, QStringList &splitted);

private:
  QMovie *local_movie;
  QMovie *remote_movie;
  std::vector <OneFile> remote_files;
  std::vector <OneFile> local_files;
  std::deque <FileToTransfer> files_to_transfer;
  QDir current_local_dir;
  QString current_remote_dir;

  Ui::DlgTransferFile *ui;

  void local_back();
  void remote_back();

  void path_local_changed(const QString &new_path);
  void path_remote_changed(const QString &new_path);

  void add_file_local(const QFileInfo &fi);
  void add_file_remote(const QString &file_info);

  void refresh_local_file_system();
  void refresh_remote_file_system();

  void file_to_upload();
  void file_to_download();

  void upload_selected();
  void download_selected();

  void refresh_button_local();
  void refresh_button_remote();

  void file_local_selected(const QModelIndex &index);
  void file_remote_selected(const QModelIndex &index);

  // common functions

  void file_was_dropped(const QString &file_path);
  void set_buttons_enabled(bool enabled);
  void transfer_finished(int tw_row, system_call_wrapper_error_t res, QStringList output);
  void transfer_file(int tw_row);
  void start_transfer_files();
  void clear_files();
  void design_table_widget(QTableWidget *tw, const QStringList &headers);
  void add_file_to_file_system_tw(QTableWidget *file_system_tw, int row, const OneFile &file);
  void file_transfer_field_add_file(const FileToTransfer &file, bool instant_transfer);
  QString parseDate(const QString &month, const QString &day, const QString &year_or_time);
  void check_more_info(bool checked);
  void output_from_remote_command(system_call_wrapper_error_t res, const QStringList &output);
  void Init();


  // Drag and Drop from left to right and vice versa
  void local_cell_pressed(int row, int column);
  void remote_cell_pressed(int row, int column);
  void local_file_system_drop();
  void remote_file_system_drop();
};

#endif // DLGTRANSFERFILE_H
