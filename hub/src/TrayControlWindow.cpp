#include <QDesktopWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <QWidgetAction>
#include <QtConcurrent/QtConcurrent>
#include <QtGui>
#include <algorithm>

#include "DlgAbout.h"
#include "DlgGenerateSshKey.h"
#include "DlgLogin.h"
#include "DlgNotification.h"
#include "DlgNotifications.h"
#include "DlgSettings.h"
#include "DlgEnvironment.h"
#include "DlgPeer.h"
#include "HubController.h"
#include "OsBranchConsts.h"
#include "RestWorker.h"
#include "SettingsManager.h"
#include "SystemCallWrapper.h"
#include "TrayControlWindow.h"
#include "libssh2/include/LibsshController.h"
#include "ui_TrayControlWindow.h"
#include "updater/HubComponentsUpdater.h"
#include "DlgEnvironment.h"
#include "P2PController.h"
#include "RhController.h"

using namespace update_system;


template<class OS> static inline void
InitTrayIconTriggerHandler_internal(QSystemTrayIcon* icon,
                                    TrayControlWindow* win);

template<>
inline void InitTrayIconTriggerHandler_internal<Os2Type<OS_WIN> >(
        QSystemTrayIcon *icon, TrayControlWindow *win) {
    QObject::connect(icon, &QSystemTrayIcon::activated,
                     win, &TrayControlWindow::tray_icon_is_activated_sl);
}

template<>
inline void InitTrayIconTriggerHandler_internal<Os2Type<OS_LINUX> >(
        QSystemTrayIcon *icon, TrayControlWindow *win) {
    UNUSED_ARG(icon);
    UNUSED_ARG(win);
}

template<>
inline void InitTrayIconTriggerHandler_internal<Os2Type<OS_MAC> >(
        QSystemTrayIcon *icon, TrayControlWindow *win) {
    UNUSED_ARG(icon);
    UNUSED_ARG(win);
}

void InitTrayIconTriggerHandler(QSystemTrayIcon *icon,
                                TrayControlWindow *win) {
  InitTrayIconTriggerHandler_internal<Os2Type<CURRENT_OS> >(icon, win);
}


TrayControlWindow::TrayControlWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::TrayControlWindow),
      m_act_ssh_keys_management(NULL),
      m_act_quit(NULL),
      m_act_settings(NULL),
      m_act_balance(NULL),
      m_act_hub(NULL),
      m_act_launch_Hub(NULL),
      m_act_about(NULL),
      m_act_logout(NULL),
      m_sys_tray_icon(NULL),
      m_tray_menu(NULL),
  /*p2p status*/    m_act_p2p_status(NULL)

{
  ui->setupUi(this);

  create_tray_actions();
  create_tray_icon();
  m_sys_tray_icon->show();



  connect(CNotificationObserver::Instance(), &CNotificationObserver::notify,
          this, &TrayControlWindow::notification_received);

  connect(&CHubController::Instance(),
          &CHubController::ssh_to_container_finished, this,
          &TrayControlWindow::ssh_to_container_finished);
  connect(&CHubController::Instance(),
          &CHubController::desktop_to_container_finished, this,
          &TrayControlWindow::desktop_to_container_finished);

  connect(&CHubController::Instance(), &CHubController::balance_updated, this,
          &TrayControlWindow::balance_updated_sl);
  connect(&CHubController::Instance(), &CHubController::environments_updated,
          this, &TrayControlWindow::environments_updated_sl);
  connect(&CHubController::Instance(), &CHubController::my_peers_updated,
          this, &TrayControlWindow::my_peers_updated_sl);

  connect(CRestWorker::Instance(), &CRestWorker::on_got_ss_console_readiness,
          this, &TrayControlWindow::got_ss_console_readiness_sl);

  connect(CHubComponentsUpdater::Instance(),
          &CHubComponentsUpdater::updating_finished, this,
          &TrayControlWindow::update_finished);
  connect(CHubComponentsUpdater::Instance(),
          &CHubComponentsUpdater::update_available, this,
          &TrayControlWindow::update_available);
  connect(CRhController::Instance(), &CRhController::ssh_to_rh_finished, this,
          &TrayControlWindow::ssh_to_rh_finished_sl);

  /*p2p status updater*/
  P2PStatus_checker *p2p_status_updater=&P2PStatus_checker::Instance();
  p2p_status_updater->update_status();
  connect(p2p_status_updater, &P2PStatus_checker::p2p_status, this,
          &TrayControlWindow::update_p2p_status_sl);

  InitTrayIconTriggerHandler(m_sys_tray_icon, this);
  CHubController::Instance().force_refresh();
  login_success();
}

TrayControlWindow::~TrayControlWindow() {
  QMenu* menus[] = {m_hub_menu, m_hub_peer_menu, m_local_peer_menu, m_tray_menu};
  QAction* acts[] = {m_act_ssh_keys_management,
                     m_act_quit,
                     m_act_settings,
                     m_act_balance,
                     m_act_hub,
                     m_act_launch_Hub,
                     m_act_about,
                     m_act_logout,
                     m_act_notifications_history,
                     m_act_p2p_status
                    };

  for (size_t i = 0; i < sizeof(menus) / sizeof(QMenu*); ++i) {
    if (menus[i] == nullptr) continue;
    try {
      delete menus[i];
    } catch (...) { /*do nothing*/
    }
  }

  for (size_t i = 0; i < sizeof(acts) / sizeof(QAction*); ++i) {
    if (acts[i] == nullptr) continue;
    try {
      delete acts[i];
    } catch (...) { /*do nothing*/
    }
  }

  try {
    delete m_sys_tray_icon;
  } catch (...) {
  }
  delete ui;
}
////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::application_quit() {
  qDebug() << "Quitting the tray";
  QApplication::quit();
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::create_tray_actions() {

  m_act_settings =
      new QAction(QIcon(":/hub/Settings-07.png"), tr("Settings"), this);
  connect(m_act_settings, &QAction::triggered, this,
          &TrayControlWindow::show_settings_dialog);



  m_act_hub =
      new QAction(QIcon(":/hub/Environmetns-07.png"), tr("Environments"), this);

  m_act_quit = new QAction(QIcon(":/hub/Exit-07"), tr("Quit"), this);
  connect(m_act_quit, &QAction::triggered, this,
          &TrayControlWindow::application_quit);

  m_act_launch_Hub =
      new QAction(QIcon(":/hub/Hub-07.png"), tr("Hub website"), this);
  connect(m_act_launch_Hub, &QAction::triggered, this,
          &TrayControlWindow::launch_Hub);

  m_act_balance = new QAction(QIcon(":/hub/Balance-07.png"),
                              CHubController::Instance().balance(), this);
  connect(m_act_balance, &QAction::triggered,
          [] { CHubController::Instance().launch_balance_page(); });

  m_act_about = new QAction(QIcon(":/hub/about.png"), tr("About"), this);
  connect(m_act_about, &QAction::triggered, this,
          &TrayControlWindow::show_about);

  m_act_ssh_keys_management =
      new QAction(QIcon(":/hub/ssh-keys.png"), tr("SSH-keys management"), this);
  connect(m_act_ssh_keys_management, &QAction::triggered, this,
          &TrayControlWindow::ssh_key_generate_triggered);

  m_act_logout = new QAction(QIcon(":/hub/logout.png"), tr("Logout"), this);
  connect(m_act_logout, &QAction::triggered, this, &TrayControlWindow::logout);

  m_act_notifications_history = new QAction(
      QIcon(":hub/notifications_history.png"), tr("Notifications history"), this);
  connect(m_act_notifications_history, &QAction::triggered, this,
          &TrayControlWindow::show_notifications_triggered);

  /*p2p status*/
  m_act_p2p_status = new QAction(
        QIcon(":hub/stopped"), tr("p2p is not launched yet"), this);
  connect(m_act_p2p_status, &QAction::triggered, this,
          &TrayControlWindow::launch_p2p);
}
////////////////////////////////////////////////////////////////////////////


void TrayControlWindow::create_tray_icon() {
  m_sys_tray_icon = new QSystemTrayIcon(this);
  m_tray_menu = new QMenu(this);
  m_sys_tray_icon->setContextMenu(m_tray_menu);
  m_tray_menu->addAction(m_act_p2p_status);
  m_tray_menu->addSeparator();

  m_tray_menu->addAction(m_act_launch_Hub);
  m_tray_menu->addAction(m_act_balance);

  m_tray_menu->addSeparator();

  m_hub_menu = m_tray_menu->addMenu(QIcon(":/hub/Environmetns-07.png"),
                                    tr("Environments"));
  m_hub_menu->setStyleSheet(qApp->styleSheet());
  m_hub_peer_menu = m_tray_menu->addMenu(QIcon(":/hub/tray.png"),
                                     tr("My Peers"));
  m_local_peer_menu = m_tray_menu->addMenu(QIcon(":/hub/Launch-07.png"),
                                     tr("Local Peers"));




  m_tray_menu->addSeparator();
  m_tray_menu->addAction(m_act_settings);
  m_tray_menu->addAction(m_act_ssh_keys_management);
  m_tray_menu->addAction(m_act_notifications_history);
  m_tray_menu->addSeparator();
  m_tray_menu->addAction(m_act_about);
  m_tray_menu->addAction(m_act_logout);
  m_tray_menu->addAction(m_act_quit);

  m_sys_tray_icon->setIcon(QIcon(":/hub/Tray_icon_set-07.png"));
}

void TrayControlWindow::get_sys_tray_icon_coordinates_for_dialog(
    int& src_x, int& src_y, int& dst_x, int& dst_y, int dlg_w, int dlg_h,
    bool use_cursor_position) {
  int icon_x, icon_y;
  dst_x = dst_y = 0;
  src_x = src_y = 0;

  icon_x = m_sys_tray_icon->geometry().x();
  icon_y = m_sys_tray_icon->geometry().y();

  int adw, adh;
  adw = QApplication::desktop()->availableGeometry().width();
  adh = QApplication::desktop()->availableGeometry().height();

  if (icon_x == 0 && icon_y == 0) {
    if (use_cursor_position) {
      icon_x = QCursor::pos().x();
      icon_y = QCursor::pos().y();
    } else {
      int coords[] = {adw, 0, adw, adh, 0, adh, 0, 0};
      uint32_t pc =
          CSettingsManager::Instance().preferred_notifications_place();
      icon_x = coords[pc * 2];
      icon_y = coords[pc * 2 + 1];
    }
  }

  int dx, dy;
  dy = QApplication::desktop()->availableGeometry().y();
  dx = QApplication::desktop()->availableGeometry().x();

#ifdef RT_OS_WINDOWS
  dy += dy ? 0 : 35;  // don't know why -20 and 35
#endif

  if (icon_x < adw / 2) {
    src_x = -dlg_w + dx;
    dst_x = src_x + dlg_w;
  } else {
    src_x = adw + dx;
    dst_x = src_x - dlg_w;
  }

  src_y = icon_y < adh / 2 ? dy : adh - dy - dlg_h;
  dst_y = src_y;
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::tray_icon_is_activated_sl(QSystemTrayIcon::ActivationReason reason) {
  if (reason == QSystemTrayIcon::Trigger) {
    m_sys_tray_icon->contextMenu()->exec(QPoint(QCursor::pos().x() ,QCursor::pos().y()));
  }
}

////////////////////////////////////////////////////////////////////////////

static QPoint lastNotificationPos(0, 0);
template<class OS>
static inline void shift_notification_dialog_positions_internal(int &src_y , int &dst_y, int shift_value);

template <>
inline void shift_notification_dialog_positions_internal< Os2Type<OS_LINUX> >(int &src_y , int &dst_y, int shift_value){
  const int &pref_place = CSettingsManager::Instance().preferred_notifications_place();
  if (pref_place == 1 || pref_place == 2) { // if the notification dialogs on the top of the screen
    src_y = lastNotificationPos.y() - shift_value;
    dst_y = lastNotificationPos.y() - shift_value;
  }
  else { // on the bottom
    src_y = lastNotificationPos.y() + shift_value;
    dst_y = lastNotificationPos.y() + shift_value;
  }
}
template<>
inline void shift_notification_dialog_positions_internal< Os2Type<OS_WIN> >(int &src_y , int &dst_y, int shift_value){
  src_y = lastNotificationPos.y() - shift_value;
  dst_y = lastNotificationPos.y() - shift_value;
}

template<>
inline void shift_notification_dialog_positions_internal< Os2Type<OS_MAC> >(int &src_y , int &dst_y, int shift_value){
  src_y = lastNotificationPos.y() + shift_value;
  dst_y = lastNotificationPos.y() + shift_value;
}

void shift_notification_dialog_positions(int &src_y, int &dst_y, int shift_value) {
  shift_notification_dialog_positions_internal< Os2Type<CURRENT_OS> >(src_y, dst_y, shift_value);
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::notification_received(
    CNotificationObserver::notification_level_t level, const QString& msg,
    DlgNotification::NOTIFICATION_ACTION_TYPE action_type) {
  qDebug()
      << "Message: " << msg
      << "Level: " << CNotificationObserver::notification_level_to_str(level)
      << "Action Type: " << (size_t)action_type
      << "Current notification level: " << CSettingsManager::Instance().notifications_level()
      << "Message is ignored: " << CSettingsManager::Instance().is_notification_ignored(msg);

  if (CSettingsManager::Instance().is_notification_ignored(msg) ||
      (uint32_t)level < CSettingsManager::Instance().notifications_level()) {
    return;
  }

  QDialog* dlg = new DlgNotification(level, msg, this, action_type);

  dlg->setWindowFlags(dlg->windowFlags() | Qt::WindowStaysOnTopHint);

  connect(dlg, &QDialog::finished, dlg, &DlgNotification::deleteLater);
  int src_x, src_y, dst_x, dst_y;
  get_sys_tray_icon_coordinates_for_dialog(src_x, src_y, dst_x, dst_y,
                                           dlg->width(), dlg->height(), false);
  if (DlgNotification::NOTIFICATIONS_COUNT > 1 && DlgNotification::NOTIFICATIONS_COUNT < 5) { // shift dialog if there is more than one dialogs
      shift_notification_dialog_positions(src_y, dst_y, dlg->height() + 20);
  }

  if (CSettingsManager::Instance().use_animations()) {
    QPropertyAnimation* pos_anim = new QPropertyAnimation(dlg, "pos");
    QPropertyAnimation* opa_anim = new QPropertyAnimation(dlg, "windowOpacity");

    pos_anim->setStartValue(QPoint(src_x, src_y));
    pos_anim->setEndValue(QPoint(dst_x, dst_y));
    pos_anim->setEasingCurve(QEasingCurve::OutBack);
    pos_anim->setDuration(800);

    opa_anim->setStartValue(0.0);
    opa_anim->setEndValue(1.0);
    opa_anim->setEasingCurve(QEasingCurve::Linear);
    opa_anim->setDuration(800);

    QParallelAnimationGroup* gr = new QParallelAnimationGroup;
    gr->addAnimation(pos_anim);
    gr->addAnimation(opa_anim);

    dlg->move(src_x, src_y);

    dlg->show();
    gr->start();
    connect(gr, &QParallelAnimationGroup::finished, gr,
            &QParallelAnimationGroup::deleteLater);
  } else {
    dlg->move(dst_x, dst_y);
    dlg->show();
  }
  lastNotificationPos = dlg->pos();
}
////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::logout() {
  std::vector<QDialog*> lstActiveDialogs(m_dct_active_dialogs.size());
  int i = 0;
  //this extra copy because on dialog finish we are removing it from m_dct_active_dialogs
  for (auto j = m_dct_active_dialogs.begin(); j != m_dct_active_dialogs.end(); ++j, ++i)
    lstActiveDialogs[i] = j->second;

  //close active dialogs
  while(i--)
    lstActiveDialogs[i]->close();

  CHubController::Instance().logout();
  this->m_sys_tray_icon->hide();

  DlgLogin dlg;
  connect(&dlg, &DlgLogin::login_success, this,
          &TrayControlWindow::login_success);
  dlg.setModal(true);
  if (dlg.exec() != QDialog::Accepted) {
    qApp->exit(0);
  }
}
////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::login_success() {
  CHubController::Instance().start();
  m_sys_tray_icon->show();
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::ssh_to_container_triggered(const CEnvironment* env,
                                                   const CHubContainer* cont,
                                                   void* action) {
  qDebug()
      << QString("Environment [name: %1, id: %2]").arg(env->name(), env->id())
      << QString("Container [name: %1, id: %2]").arg(cont->name(), cont->id());

  QPushButton* act = static_cast<QPushButton*>(action);
  if (act != NULL) {
    act->setEnabled(false);
    act->setText("PROCESSSING...");
    CHubController::Instance().ssh_to_container(env, cont, action);
  }
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::ssh_to_rh_triggered(const QString &peer_fingerprint, void* action) {
  qDebug()
      << QString("Peer [peer_fingerprint: %1]").arg(peer_fingerprint);

  QPushButton* act = static_cast<QPushButton*>(action);
  if (act != NULL) {
    act->setEnabled(false);
    act->setText("PROCESSSING...");
    QtConcurrent::run(CRhController::Instance(), &CRhController::ssh_to_rh
                      , peer_fingerprint, action);
  }
}

void TrayControlWindow::ssh_to_rh_finished_sl(const QString &peer_fingerprint, void *action, system_call_wrapper_error_t res, int libbssh_exit_code) {
  qDebug()
      << QString("Peer [peer_fingerprint: %1]").arg(peer_fingerprint);

  QPushButton* act = static_cast<QPushButton*>(action);
  if (act != NULL) {
    act->setText("Save && SSH into Peer");
    act->setEnabled(true);
    if (res != SCWE_SUCCESS)
    {
      if (libbssh_exit_code != 0)
        CNotificationObserver::Info(tr("This Peer is not accessible with provided credentials. Please check and verify. Error SSH code: %1").arg(CLibsshController::run_libssh2_error_to_str((run_libssh2_error_t)libbssh_exit_code)),
                                    DlgNotification::N_NO_ACTION);
      else
        CNotificationObserver::Info(tr("Can't run terminal to ssh into peer. Error code: %1").arg(CSystemCallWrapper::scwe_error_to_str(res)),
                                    DlgNotification::N_NO_ACTION);
    }
  }
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::desktop_to_container_triggered(const CEnvironment* env,
                                                   const CHubContainer* cont,
                                                   void* action) {
  qDebug()
      << QString("Environment [name: %1, id: %2]").arg(env->name(), env->id())
      << QString("Container [name: %1, id: %2]").arg(cont->name(), cont->id())
      << QString("X2go is Launchable: %1").arg(CSystemCallWrapper::x2goclient_check());
  if (!CSystemCallWrapper::x2goclient_check()) {
    CNotificationObserver::Error(QObject::tr("Can't run x2goclient instance. Make sure you have specified correct path to x2goclient."
                                         "Or you can get the lasest x2goclient from <a href=\"%2\">here</a>.")
                                 .arg(x2goclient_url()), DlgNotification::N_SETTINGS);
    return;
  }

  QPushButton* act = static_cast<QPushButton*>(action);
  if (act != NULL) {
    act->setEnabled(false);
    act->setText("PROCESSSING...");
    CHubController::Instance().desktop_to_container(env, cont, action);
  }
}


////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::update_available(QString file_id) {
  qDebug() << "File ID: " << file_id;
  CNotificationObserver::Info(
      tr("Update for %1 is available. Check \"About\" dialog").arg(file_id), DlgNotification::N_ABOUT);
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::update_finished(QString file_id, bool success) {
  qDebug() << QString("File ID: %1, Success: %2").arg(file_id, success);
  if (!success) {
    CNotificationObserver::Error(
        tr("Failed to update %1. See details in error logs").arg(file_id), DlgNotification::N_NO_ACTION);
    return;
  }
}
////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::launch_Hub() {
  CHubController::Instance().launch_browser(hub_site());
}
////////////////////////////////////////////////////////////////////////////

/*p2p status */
void TrayControlWindow::launch_p2p(){
   CHubController::Instance().launch_browser("https://subutai.io/install/index.html");
}

//////////////
void TrayControlWindow::environments_updated_sl(int rr) {
  qDebug()
      << "Updating environments list"
      << "Result: " << rr;



  static QIcon unhealthy_icon(":/hub/BAD.png");
  static QIcon healthy_icon(":/hub/GOOD.png");
  static QIcon modification_icon(":/hub/OK.png");

  UNUSED_ARG(rr);
  static std::vector<QString> lst_checked_unhealthy_env;
  m_hub_menu->clear();

  std::vector<QString> lst_unhealthy_envs;
  std::vector<QString> lst_unhealthy_env_statuses;

  if (CHubController::Instance().lst_environments().empty()) {
    QAction* empty_action = new QAction("Empty", this);
    empty_action->setEnabled(false);
    m_hub_menu->addAction(empty_action);
    return;
  }

  for (auto env = CHubController::Instance().lst_environments().cbegin();
       env != CHubController::Instance().lst_environments().cend(); ++env) {
    QString env_name = env->name();
    QAction* env_start = m_hub_menu->addAction(env->name());
    env_start->setIcon(env->status() == "HEALTHY" ? healthy_icon :
                          env->status() == "UNHEALTHY" ? unhealthy_icon : modification_icon);

    std::vector<QString>::iterator iter_found =
        std::find(lst_checked_unhealthy_env.begin(),
                  lst_checked_unhealthy_env.end(), env->id());

    if (!env->healthy()) {
      if (iter_found == lst_checked_unhealthy_env.end()) {
        lst_unhealthy_envs.push_back(env_name);
        lst_unhealthy_env_statuses.push_back(env->status());
        lst_checked_unhealthy_env.push_back(env->id());
        qCritical(
            "Environment %s, %s is unhealthy. Reason : %s",
            env_name.toStdString().c_str(), env->id().toStdString().c_str(),
            env->status_description().toStdString().c_str());
      }
    } else {
      if (iter_found != lst_checked_unhealthy_env.end()) {
        CNotificationObserver::Info(
            tr("Environment %1 became healthy").arg(env->name()), DlgNotification::N_NO_ACTION);
        qInfo(
            "Environment %s became healthy", env->name().toStdString().c_str());
        lst_checked_unhealthy_env.erase(iter_found);
      }
      qInfo(
          "Environment %s is healthy", env->name().toStdString().c_str());
    }

    connect(env_start, &QAction::triggered, [env, this](){
      this->generate_env_dlg(&(*env));
      TrayControlWindow::show_dialog(TrayControlWindow::last_generated_env_dlg,
                                     QString("Environment \"%1\" (%2)").arg(env->name()).arg(env->status()));
    });
  }  // for auto env in environments list

  if (lst_unhealthy_envs.empty()) return;

  QString str_unhealthy_envs = "";
  QString str_statuses = "";
  for (size_t i = 0; i < lst_unhealthy_envs.size() - 1; ++i) {
    str_unhealthy_envs += lst_unhealthy_envs[i] + ", ";
    str_statuses += lst_unhealthy_env_statuses[i] + ", ";
  }

  str_unhealthy_envs += lst_unhealthy_envs[lst_unhealthy_envs.size() - 1];
  str_statuses += lst_unhealthy_env_statuses[lst_unhealthy_envs.size() - 1];
  qDebug()
      << QString("Unhealthy Environments: %1 with statuses: %2").arg(str_unhealthy_envs, str_statuses);

  QString str_notification =
      tr("Environment%1 %2 %3 %4")
          .arg(lst_unhealthy_envs.size() > 1 ? "s" : "")
          .arg(str_unhealthy_envs)
          .arg(lst_unhealthy_envs.size() > 1 ? "are" : "is")
          .arg(str_statuses);

  CNotificationObserver::Instance()->Info(str_notification, DlgNotification::N_NO_ACTION);
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::my_peers_updated_sl() {

  std::vector<CMyPeerInfo> my_current_peers = CHubController::Instance().lst_my_peers();
  QString msgDisconnected = "";
  QString msgConnected = "";

  /// check if some peers were disconnected or deleted
  for (CMyPeerInfo peer : peers_connected) {
    std::vector<CMyPeerInfo>::iterator found_peer = std::find_if(my_current_peers.begin(), my_current_peers.end(),
                                                    [peer](const CMyPeerInfo &p){return peer.id() == p.id();});
    if(found_peer == my_current_peers.end()) // it was disconnected or deleted
      msgDisconnected +=
          tr("\"%1\" is %2 ")
            .arg(peer.name())
            .arg("disconnected");
  }

  if (!msgDisconnected.isEmpty() && !msgDisconnected.isNull()) {
    qDebug() << "Disconnected Peer Message: " << msgDisconnected;
    CNotificationObserver::Instance()->Info(tr("Status of your Peers: ") + msgDisconnected, DlgNotification::N_GO_TO_HUB);
  }

  /// check if some new peers were connected or changed status
  for (CMyPeerInfo peer : my_current_peers) {
    std::vector<CMyPeerInfo>::iterator found_peer = std::find_if(peers_connected.begin(), peers_connected.end(),
                                                    [peer](const CMyPeerInfo &p){return peer.id() == p.id();});
    if(found_peer == peers_connected.end() || found_peer->status() != peer.status()) { // new connected or changed status
      msgConnected +=
          tr("\"%1\" is %2 ")
             .arg(peer.name())
             .arg(peer.status());
    }
  }
  if (!msgConnected.isEmpty() && !msgConnected.isNull()) {
    qDebug() << "Connected Peer Message: " << msgConnected;
    CNotificationObserver::Instance()->Info(tr("Status of your Peers: ") + msgConnected, DlgNotification::N_GO_TO_HUB);
  }
  peers_connected = my_current_peers;
  update_peer_menu();
}

void TrayControlWindow::update_peer_menu() {
  static QIcon online_icon(":/hub/GOOD.png");
  static QIcon offline_icon(":/hub/BAD.png");
  static QIcon unknown_icon(":/hub/OK.png");
  static QIcon local_hub(":/hub/local_hub.png");
  static QIcon local_network_icon(":/hub/local-network.png");


  m_hub_peer_menu->clear();
  m_local_peer_menu->clear();


  // migrate from dct_resource_hosts to found_local_peers by modifying uid to fingerprint
  std::vector <std::pair<QString, QString>> found_local_peers;
  for (std::pair<QString, QString> local_peer : CRhController::Instance()->dct_resource_hosts()) {
    found_local_peers.push_back(std::make_pair(CCommons::GetFingerprintFromUid(local_peer.first), local_peer.second));
  }

  // Find local&hub peers
  for (std::pair<QString, QString> local_peer : found_local_peers) {
    for (auto hub_peer = peers_connected.begin() ; hub_peer != peers_connected.end() ; hub_peer ++) {
        int eq = QString::compare(local_peer.first, hub_peer->fingerprint(), Qt::CaseInsensitive);
        if (eq == 0) // found peer both local and registered on hub
        {
          QAction *peer_start = m_hub_peer_menu->addAction(hub_peer->name() + " - " + local_peer.second);
          peer_start->setIcon(local_hub);

          connect(peer_start, &QAction::triggered, [local_peer, hub_peer, this](){
            this->generate_peer_dlg(&(*hub_peer), local_peer);
            TrayControlWindow::show_dialog(TrayControlWindow::last_generated_peer_dlg,
                                           QString("Peer \"%1\" - %2(%3)").arg(hub_peer->name()).arg(local_peer.second).arg(hub_peer->status()));
          });
          break;
        }
    }
  }

  // Find local peers
  for (std::pair<QString, QString> local_peer : found_local_peers) {
    bool found_on_hub = false;
    for (auto hub_peer = peers_connected.begin() ; hub_peer != peers_connected.end() ; hub_peer ++) {
      int eq = QString::compare(local_peer.first, hub_peer->fingerprint(), Qt::CaseInsensitive);
      if (eq == 0) // found peer on hub
      {
        found_on_hub = true;
        break;
      }
    }

    if (found_on_hub == false && local_peer.first != QString("current_setting")) {
      QAction *peer_start = m_local_peer_menu->addAction(local_peer.second);
      peer_start->setIcon(local_network_icon);
      connect(peer_start, &QAction::triggered, [this, local_peer]() {
        this->generate_peer_dlg(NULL, local_peer);
        TrayControlWindow::show_dialog(TrayControlWindow::last_generated_peer_dlg,
                                       QString("Peer \"%1\"").arg(local_peer.second));
      });
    }
  }

  // Find hub peers
  for (auto hub_peer = peers_connected.begin() ; hub_peer != peers_connected.end() ; hub_peer ++) {
    bool found_on_local = false;
    for (std::pair<QString, QString> local_peer : found_local_peers) {
      int eq = QString::compare(local_peer.first, hub_peer->fingerprint(), Qt::CaseInsensitive);
      if (eq == 0) // found peer on local
      {
        found_on_local = true;
        break;
      }
    }
    if (found_on_local == false) {
      QAction *peer_start = m_hub_peer_menu->addAction(hub_peer->name());
      peer_start->setIcon(hub_peer->status() == "ONLINE" ? online_icon :
                            hub_peer->status() == "OFFLINE" ? offline_icon : unknown_icon);
      connect(peer_start, &QAction::triggered, [hub_peer, this](){
        this->generate_peer_dlg(&(*hub_peer), std::make_pair("",""));
        TrayControlWindow::show_dialog(TrayControlWindow::last_generated_peer_dlg,
                                       QString("Peer \"%1\"(%2)").arg(hub_peer->name()).arg(hub_peer->status()));
      });
    }
  }

  if (m_hub_peer_menu->isEmpty()) {
    m_hub_peer_menu->addAction("Empty")->setEnabled(false);
  }
  if (m_local_peer_menu->isEmpty()) {
    m_local_peer_menu->addAction("Empty")->setEnabled(false);
  }
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::balance_updated_sl() {
  m_act_balance->setText(CHubController::Instance().balance());
}

////////////////////////////////////////////////////////////////////////////

/* p2p status updater*/
void TrayControlWindow::update_p2p_status_sl(P2PStatus_checker::P2P_STATUS status){
    switch(status){
        case P2PStatus_checker::P2P_READY :
            m_act_p2p_status->setText("P2P is not running");
            m_act_p2p_status->setIcon(QIcon(":/hub/waiting"));
            break;
        case P2PStatus_checker::P2P_RUNNING :
            m_act_p2p_status->setText("P2P is running");
            m_act_p2p_status->setIcon(QIcon(":/hub/running"));
            break;
        case P2PStatus_checker::P2P_FAIL :
            m_act_p2p_status->setText("Cannot find P2P");
            m_act_p2p_status->setIcon(QIcon(":/hub/stopped"));
            break;
    }
}

void TrayControlWindow::got_ss_console_readiness_sl(bool is_ready,
                                                    QString err) {
  qDebug()
      << "Is console ready: " << is_ready
      << "Error: " << err;
  if (!is_ready) {
    CNotificationObserver::Info(tr(err.toStdString().c_str()), DlgNotification::N_NO_ACTION);
    return;
  }

  QString hub_url = "https://localhost:9999";

  std::string rh_ip;
  int ec = 0;

  system_call_wrapper_error_t scwe = CSystemCallWrapper::get_rh_ip_via_libssh2(
      CSettingsManager::Instance().rh_host(m_default_peer_id).toStdString().c_str(),
      CSettingsManager::Instance().rh_port(m_default_peer_id),
      CSettingsManager::Instance().rh_user(m_default_peer_id).toStdString().c_str(),
      CSettingsManager::Instance().rh_pass(m_default_peer_id).toStdString().c_str(), ec, rh_ip);

  if (scwe == SCWE_SUCCESS && (ec == RLE_SUCCESS || ec == 0)) {
    hub_url = QString("https://%1:8443").arg(rh_ip.c_str());
  } else {
    qCritical(
        "Can't get RH IP address. Err : %s",
        CLibsshController::run_libssh2_error_to_str((run_libssh2_error_t)ec));
    CNotificationObserver::Info(
        tr("Can't get RH IP address. Error : %1")
            .arg(CLibsshController::run_libssh2_error_to_str((run_libssh2_error_t)ec)), DlgNotification::N_NO_ACTION);
    return;
  }

  CHubController::Instance().launch_browser(hub_url);
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::launch_ss() {
  std::string rh_ip;
  int ec = 0;
  system_call_wrapper_error_t scwe = CSystemCallWrapper::get_rh_ip_via_libssh2(
      CSettingsManager::Instance().rh_host(m_default_peer_id).toStdString().c_str(),
      CSettingsManager::Instance().rh_port(m_default_peer_id),
      CSettingsManager::Instance().rh_user(m_default_peer_id).toStdString().c_str(),
      CSettingsManager::Instance().rh_pass(m_default_peer_id).toStdString().c_str(), ec, rh_ip);

  if (scwe == SCWE_SUCCESS && (ec == RLE_SUCCESS || ec == 0)) {
    QString tmp =
        QString("https://%1:8443/rest/v1/peer/ready").arg(rh_ip.c_str());
    // after that got_ss_console_readiness_sl will be called
    qInfo("launch_ss : %s", tmp.toStdString().c_str());
    CRestWorker::Instance()->check_if_ss_console_is_ready(tmp);
  } else {
    qCritical(
        "Can't get RH IP address. Err : %s",
        CLibsshController::run_libssh2_error_to_str((run_libssh2_error_t)ec));
    CNotificationObserver::Info(tr("Can't get RH IP address. Error : %1")
            .arg(CLibsshController::run_libssh2_error_to_str((run_libssh2_error_t)ec)), DlgNotification::N_NO_ACTION);
  }
}
////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::show_dialog(QDialog* (*pf_dlg_create)(QWidget*),
                                    const QString& title) {
  std::map<QString, QDialog*>::iterator iter = m_dct_active_dialogs.find(title);
  qDebug() << "Poping up the dialog with title: " << title;
  if (iter == m_dct_active_dialogs.end()) {
    QDialog* dlg = pf_dlg_create(this);
    dlg->setWindowTitle(title);
    m_dct_active_dialogs[dlg->windowTitle()] = dlg;

    int src_x, src_y, dst_x, dst_y;
    get_sys_tray_icon_coordinates_for_dialog(src_x, src_y, dst_x, dst_y,
                                             dlg->width(), dlg->height(), true);

    if (CSettingsManager::Instance().use_animations()) {
      QPropertyAnimation* pos_anim = new QPropertyAnimation(dlg, "pos");
      QPropertyAnimation* opa_anim =
          new QPropertyAnimation(dlg, "windowOpacity");

      pos_anim->setStartValue(QPoint(src_x, src_y));
      pos_anim->setEndValue(QPoint(dst_x, dst_y));
      pos_anim->setEasingCurve(QEasingCurve::OutBack);
      pos_anim->setDuration(800);

      opa_anim->setStartValue(0.0);
      opa_anim->setEndValue(1.0);
      opa_anim->setEasingCurve(QEasingCurve::Linear);
      opa_anim->setDuration(800);

      QParallelAnimationGroup* gr = new QParallelAnimationGroup;
      gr->addAnimation(pos_anim);
      gr->addAnimation(opa_anim);

      dlg->move(src_x, src_y);
      dlg->show();
      gr->start();
      connect(gr, &QParallelAnimationGroup::finished, [dlg]() {
        dlg->activateWindow();
        dlg->raise();
        dlg->setFocus();
      });
      connect(gr, &QParallelAnimationGroup::finished, gr,
              &QParallelAnimationGroup::deleteLater);
    } else {
      dlg->move(dst_x, dst_y);
      dlg->show();
      dlg->activateWindow();
      dlg->raise();
      dlg->setFocus();
    }
    connect(dlg, &QDialog::finished, this, &TrayControlWindow::dialog_closed);
  } else {
    if (iter->second) {
      iter->second->show();
      iter->second->activateWindow();
      iter->second->raise();
      iter->second->setFocus();
    }
  }
}
////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::dialog_closed(int unused) {
  UNUSED_ARG(unused);
  QDialog* dlg = qobject_cast<QDialog*>(sender());
  if (dlg == nullptr) return;
  QString title = dlg->windowTitle();
  dlg->deleteLater();
  auto iter = m_dct_active_dialogs.find(title);
  if (iter == m_dct_active_dialogs.end()) return;
  m_dct_active_dialogs.erase(iter);
}

////////////////////////////////////////////////////////////////////////////

QDialog* create_settings_dialog(QWidget* p) { return new DlgSettings(p); }
void TrayControlWindow::show_settings_dialog() {
  show_dialog(create_settings_dialog, tr("Settings"));
}
////////////////////////////////////////////////////////////////////////////

QDialog* create_about_dialog(QWidget* p) { return new DlgAbout(p); }
void TrayControlWindow::show_about() {
  show_dialog(create_about_dialog, tr("About Subutai Tray"));
}

////////////////////////////////////////////////////////////////////////////

QDialog* create_ssh_key_generate_dialog(QWidget* p) {
  return new DlgGenerateSshKey(p);
}
void TrayControlWindow::ssh_key_generate_triggered() {
  show_dialog(create_ssh_key_generate_dialog, tr("SSH Key Manager"));
}

QDialog* create_notifications_dialog(QWidget* p) {
  return new DlgNotifications(p);
}
void TrayControlWindow::show_notifications_triggered() {
  show_dialog(create_notifications_dialog, tr("Notifications history"));
}


QDialog* TrayControlWindow::m_last_generated_env_dlg = NULL;

QDialog* TrayControlWindow::last_generated_env_dlg(QWidget *p) {
  UNUSED_ARG(p);
  return m_last_generated_env_dlg;
}

void TrayControlWindow::generate_env_dlg(const CEnvironment *env){
  qDebug()
      << "Generating environment dialog... \n"
      << "Environment name: " << env->name();
  DlgEnvironment *dlg_env = new DlgEnvironment();
  dlg_env->addEnvironment(env);
  connect(dlg_env, &DlgEnvironment::ssh_to_container_sig, this, &TrayControlWindow::ssh_to_container_triggered);
  connect(dlg_env, &DlgEnvironment::desktop_to_container_sig, this, &TrayControlWindow::desktop_to_container_triggered);
  m_last_generated_env_dlg = dlg_env;
}
////////////////////////////////////////////////////////////////////////////

QDialog* TrayControlWindow::m_last_generated_peer_dlg = NULL;

QDialog* TrayControlWindow::last_generated_peer_dlg(QWidget *p) {
  UNUSED_ARG(p);
  return m_last_generated_peer_dlg;
}

void TrayControlWindow::generate_peer_dlg(CMyPeerInfo *peer, std::pair<QString, QString> local_peer){ // local_peer -> pair of fingerprint and local ip
  DlgPeer *dlg_peer = new DlgPeer();
  dlg_peer->addPeer(peer , local_peer);
  connect(dlg_peer, &DlgPeer::ssh_to_rh_sig, this, &TrayControlWindow::ssh_to_rh_triggered);
  m_last_generated_peer_dlg = dlg_peer;
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::ssh_to_container_finished(int result,
                                                  void* additional_data) {
  if (result != SDLE_SUCCESS) {
    CNotificationObserver::Error(
        tr("Can't ssh to container. Err : %1")
            .arg(CHubController::ssh_desktop_launch_err_to_str(result)), DlgNotification::N_NO_ACTION);
  }

  QPushButton* act = static_cast<QPushButton*>(additional_data);
  if (act == NULL) return;
  act->setEnabled(true);
  act->setText("SSH");
}

////////////////////////////////////////////////////////////////////////////

void TrayControlWindow::desktop_to_container_finished(int result,
                                                  void* additional_data) {
  qDebug() << "Result " << result;
  if (result != SDLE_SUCCESS) {
    CNotificationObserver::Error(
        tr("Can't desktop to container. Err : %1")
            .arg(CHubController::ssh_desktop_launch_err_to_str(result)), DlgNotification::N_NO_ACTION);
  }

  QPushButton* act = static_cast<QPushButton*>(additional_data);
  if (act == NULL) return;
  act->setEnabled(true);
  act->setText("DESKTOP");
}
////////////////////////////////////////////////////////////////////////////
