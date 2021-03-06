#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

#include "Commons.h"
#include "DownloadFileManager.h"
#include "NotificationObserver.h"
#include "OsBranchConsts.h"
#include "P2PController.h"
#include "RestWorker.h"
#include "SystemCallWrapper.h"
#include "updater/ExecutableUpdater.h"
#include "updater/HubComponentsUpdater.h"
#include "updater/UpdaterComponentVagrantVMwareUtility.h"

CUpdaterComponentVagrantVMwareUtility::CUpdaterComponentVagrantVMwareUtility() {
  this->m_component_id = VMWARE_UTILITY;
}

CUpdaterComponentVagrantVMwareUtility::~CUpdaterComponentVagrantVMwareUtility() {}

QString CUpdaterComponentVagrantVMwareUtility::download_vmware_utility_path() {
  QStringList lst_temp =
      QStandardPaths::standardLocations(QStandardPaths::TempLocation);
  return (lst_temp.isEmpty() ? QApplication::applicationDirPath()
                             : lst_temp[0]);
}

bool CUpdaterComponentVagrantVMwareUtility::update_available_internal() {
  QString version = "";
  CSystemCallWrapper::vmware_utility_version(version);
  return version == "undefined";
}

chue_t CUpdaterComponentVagrantVMwareUtility::install_internal() {
  QString version = "undefined";
  qDebug() << "Starting installation vagrant-vmware-utility";

  QMessageBox *msg_box = new QMessageBox(
      QMessageBox::Information, QObject::tr("Attention!"),
      QObject::tr(
          "The Vagrant VMware Utility provides the Vagrant VMware provider plugin"
          " access to various VMware functionalities."
          " The Vagrant VMware Utility is required by "
          " the Vagrant VMware Desktop provider plugin.\n"
          "Do you want to proceed?"),
      QMessageBox::Yes | QMessageBox::No);

  QObject::connect(msg_box, &QMessageBox::finished, msg_box,
                   &QMessageBox::deleteLater);
  if (msg_box->exec() != QMessageBox::Yes) {
    install_finished_sl(false, version);
    return CHUE_SUCCESS;
  }

  QString file_name = vmware_utility_kurjun_package_name();
  QString file_dir = download_vmware_utility_path();
  QString str_vmware_downloaded_path = file_dir + QDir::separator() + file_name;

  std::vector<CGorjunFileInfo> fi =
      CRestWorker::Instance()->get_gorjun_file_info(file_name);

  if (fi.empty()) {
    qCritical("File %s isn't presented on kurjun",
              m_component_id.toStdString().c_str());
    install_finished_sl(false, version);
    return CHUE_NOT_ON_KURJUN;
  }

  std::vector<CGorjunFileInfo>::iterator item = fi.begin();

  CDownloadFileManager *dm = new CDownloadFileManager(
      item->name(), str_vmware_downloaded_path, item->size());
  dm->set_link(ipfs_download_url().arg(item->id(), item->name()));

  SilentInstaller *silent_installer = new SilentInstaller(this);
  silent_installer->init(file_dir, file_name, CC_VMWARE_UTILITY);

  connect(dm, &CDownloadFileManager::download_progress_sig,
          [this](qint64 rec, qint64 total) {
            update_progress_sl(rec, total);
          });
  connect(dm, &CDownloadFileManager::finished,
          [this, silent_installer](bool success) {
            if (!success) {
              silent_installer->outputReceived(success, "undefined");
            } else {
              this->update_progress_sl(0,0);
              CNotificationObserver::Instance()->Info(
                  tr("Running installation scripts might be take too long time please wait."),
                  DlgNotification::N_NO_ACTION);
              silent_installer->startWork();
            }
          });
  connect(silent_installer, &SilentInstaller::outputReceived, this,
          &CUpdaterComponentVagrantVMwareUtility::install_finished_sl);
  connect(silent_installer, &SilentInstaller::outputReceived, dm,
          &CDownloadFileManager::deleteLater);

  dm->start_download();

  return CHUE_SUCCESS;
}

chue_t CUpdaterComponentVagrantVMwareUtility::update_internal() {
  update_progress_sl(100, 100);
  update_finished_sl(true);
  return CHUE_SUCCESS;
}

chue_t CUpdaterComponentVagrantVMwareUtility::uninstall_internal() {
  update_progress_sl(50, 100);
  static QString empty_string = "";

  SilentUninstaller *silent_uninstaller = new SilentUninstaller(this);
  silent_uninstaller->init(empty_string, empty_string, CC_VMWARE_UTILITY);

  connect(silent_uninstaller, &SilentUninstaller::outputReceived, this,
          &CUpdaterComponentVagrantVMwareUtility::uninstall_finished_sl);

  silent_uninstaller->startWork();

  return CHUE_SUCCESS;
}

void CUpdaterComponentVagrantVMwareUtility::update_post_action(bool success) {
  UNUSED_ARG(success);
}

void CUpdaterComponentVagrantVMwareUtility::install_post_internal(bool success) {
  if (!success) {
    CNotificationObserver::Instance()->Info(
        tr("Failed to install the Vagrant VMware Utility. You may try manually "
           "installing it "
           "or try again by restarting the Control Center first."),
        DlgNotification::N_NO_ACTION);
  } else {
    CNotificationObserver::Instance()->Info(
        tr("Vagrant VMware Utility has been installed successfully."),
        DlgNotification::N_NO_ACTION);
  }
}

void CUpdaterComponentVagrantVMwareUtility::uninstall_post_internal(bool success) {
  if (!success) {
    CNotificationObserver::Instance()->Info(
        tr("Failed to uninstall the Vagrant VMware Utility. You may try "
           "manually uninstalling it "
           "or try again by restarting the Control Center first."),
        DlgNotification::N_NO_ACTION);
  } else {
    CNotificationObserver::Instance()->Info(
        tr("Vagrant VMware Utility has been uninstalled successfully."),
        DlgNotification::N_NO_ACTION);
  }
}
