#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>

#include "ApplicationLog.h"
#include "NotificationObserver.h"
#include "OsBranchConsts.h"
#include "SettingsManager.h"
#include "SystemCallWrapper.h"
#include "updater/HubComponentsUpdater.h"

const QString CSettingsManager::ORG_NAME("subutai");
const QString CSettingsManager::APP_NAME("tray");

const QString CSettingsManager::SM_LOGIN("Login");
const QString CSettingsManager::SM_PASSWORD("Password");
const QString CSettingsManager::SM_REMEMBER_ME("Remember_Me");

const QString CSettingsManager::SM_REFRESH_TIME("Refresh_Time_Sec");
const QString CSettingsManager::SM_P2P_PATH("P2P_Path");

const QString CSettingsManager::SM_NOTIFICATION_DELAY_SEC(
    "Notification_Delay_Sec");
const QString CSettingsManager::SM_PLUGIN_PORT("Plugin_Port");
const QString CSettingsManager::SM_SSH_PATH("Ssh_Path");
const QString CSettingsManager::SM_SSH_USER("Ssh_User");

const QString CSettingsManager::SM_RH_USER("Rh_User");
const QString CSettingsManager::SM_RH_PASS("Rh_Pass");
const QString CSettingsManager::SM_RH_HOST("Rh_Host");
const QString CSettingsManager::SM_RH_PORT("Rh_Port");

const QString CSettingsManager::SM_LOGS_STORAGE("Rh_Logs_Storage");
const QString CSettingsManager::SM_SSH_KEYS_STORAGE("Rh_Ssh_Keys_Storage");

const QString CSettingsManager::SM_TRAY_GUID("Tray_Guid");

const QString CSettingsManager::SM_P2P_UPDATE_FREQ("P2p_update_freq");
const QString CSettingsManager::SM_RH_UPDATE_FREQ("Rh_update_freq");
const QString CSettingsManager::SM_TRAY_UPDATE_FREQ("Tray_update_freq");
const QString CSettingsManager::SM_RHMANAGEMENT_FREQ(
    "Rh_management_update_freq");
const QString CSettingsManager::SM_P2P_AUTOUPDATE("P2p_Autoupdate");
const QString CSettingsManager::SM_RH_AUTOUPDATE("Rh_Autoupdate");
const QString CSettingsManager::SM_TRAY_AUTOUPDATE("Tray_Autoupdate");
const QString CSettingsManager::SM_RHMANAGEMENT_AUTOUPDATE(
    "Rh_Management_Autoupdate");

const QString CSettingsManager::SM_RTM_DB_DIR("Rtm_Db_Dir");

const QString CSettingsManager::SM_TERMINAL_CMD("Terminal_Cmd");
const QString CSettingsManager::SM_TERMINAL_ARG("Terminal_Arg");
const QString CSettingsManager::SM_VBOXMANAGE_PATH("VBoxManage_Path");
const QString CSettingsManager::SM_DCT_NOTIFICATIONS_IGNORE(
    "Dct_Notifications_Ignored");

const QString CSettingsManager::SM_NOTIFICATIONS_LEVEL("Notifications_Level");
const QString CSettingsManager::SM_USE_ANIMATIONS(
    "Use_Animations_On_Standard_Dialogs");
const QString CSettingsManager::SM_PREFERRED_NOTIFICATIONS_PLACE(
    "Preffered_Notifications_Place");
const QString CSettingsManager::SM_SSH_KEYGEN_CMD("Ssh_Keygen_Cmd");

const QString CSettingsManager::SM_AUTOSTART("Autostart");
const QString CSettingsManager::SM_CHROME_PATH("ChromePath");
const QString CSettingsManager::SM_SUBUTAI_CMD("SubutaiCmd");

struct setting_val_t {
  void* field;
  QString val;
  void (*pf_qvar_to_T)(const QVariant&, void*);
};

static void qvar_to_bool(const QVariant& var, void* field) {
  *((bool*)field) = var.toBool();
}

static void qvar_to_int(const QVariant& var, void* field) {
  *((uint32_t*)field) = var.toUInt();
}

static void qvar_to_str(const QVariant& var, void* field) {
  if (!var.isNull() && !var.toString().isEmpty())
    *((QString*)field) = var.toString();
}

static void qvar_to_byte_arr(const QVariant& var, void* field) {
  *((QByteArray*)field) = var.toByteArray();
}

static void qvar_to_map_string_qvariant(const QVariant& var, void* field) {
  *((QMap<QString, QVariant>*)field) = var.toMap();
}
////////////////////////////////////////////////////////////////////////////

static const int def_timeout = 20;
static const QString settings_file = "subutai_tray.ini";

CSettingsManager::CSettingsManager()
    : m_settings(QApplication::applicationDirPath() + QDir::separator() +
                     settings_file,
                 QSettings::IniFormat),
      m_password_str(""),
      m_remember_me(false),
      m_refresh_time_sec(def_timeout),
      m_p2p_path(default_p2p_path()),
      m_notification_delay_sec(7),
      m_plugin_port(9998),
      m_ssh_path(ssh_cmd_path()),
      m_ssh_user("root"),

      m_rh_host("127.0.0.1"),
      m_rh_user("subutai"),
      m_rh_pass("ubuntai"),
      m_rh_port(4567),

      m_logs_storage(QApplication::applicationDirPath()),
      m_ssh_keys_storage(QApplication::applicationDirPath()),
      m_tray_guid(""),
      m_p2p_update_freq(UF_MIN30),
      m_rh_update_freq(UF_MIN30),
      m_tray_update_freq(UF_MIN30),
      m_rh_management_update_freq(UF_MIN30),
      m_p2p_autoupdate(true),
      m_rh_autoupdate(true),
      m_tray_autoupdate(true),
      m_rh_management_autoupdate(true),
      m_terminal_cmd(default_terminal()),
      m_terminal_arg(default_term_arg()),
      m_vboxmanage_path(vboxmanage_command_str()),
      m_notifications_level(CNotificationObserver::NL_INFO),
      m_use_animations(true),
      m_preferred_notifications_place(CNotificationObserver::NPP_RIGHT_UP),
      m_ssh_keygen_cmd(ssh_keygen_cmd_path()),
      m_autostart(true),
      m_chrome_path(default_chrome_path()),
      m_subutai_cmd(subutai_command()) {
  static const char* FOLDERS_TO_CREATE[] = {".ssh", ".rtm_tray", nullptr};
  QString* fields[] = {&m_ssh_keys_storage, &m_rtm_db_dir, nullptr};

  QStringList lst_home =
      QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
  if (!lst_home.empty()) {
    QString home_folder = lst_home[0];

    for (int i = 0; FOLDERS_TO_CREATE[i]; ++i) {
      QString dir_path = home_folder + QDir::separator() + FOLDERS_TO_CREATE[i];
      QDir dir(dir_path);
      if (!dir.exists()) {
        if (!dir.mkdir(dir_path)) continue;
      }
      *fields[i] = dir_path;
    }
  }

  setting_val_t dct_settings_vals[] = {
      // str
      {(void*)&m_login, SM_LOGIN, qvar_to_str},
      {(void*)&m_p2p_path, SM_P2P_PATH, qvar_to_str},
      {(void*)&m_ssh_path, SM_SSH_PATH, qvar_to_str},
      {(void*)&m_ssh_user, SM_SSH_USER, qvar_to_str},
      {(void*)&m_rh_host, SM_RH_HOST, qvar_to_str},
      {(void*)&m_rh_pass, SM_RH_PASS, qvar_to_str},
      {(void*)&m_rh_user, SM_RH_USER, qvar_to_str},
      {(void*)&m_logs_storage, SM_LOGS_STORAGE, qvar_to_str},
      {(void*)&m_ssh_keys_storage, SM_SSH_KEYS_STORAGE, qvar_to_str},
      {(void*)&m_tray_guid, SM_TRAY_GUID, qvar_to_str},
      {(void*)&m_rtm_db_dir, SM_RTM_DB_DIR, qvar_to_str},
      {(void*)&m_terminal_cmd, SM_TERMINAL_CMD, qvar_to_str},
      {(void*)&m_terminal_arg, SM_TERMINAL_ARG, qvar_to_str},
      {(void*)&m_vboxmanage_path, SM_VBOXMANAGE_PATH, qvar_to_str},
      {(void*)&m_ssh_keygen_cmd, SM_SSH_KEYGEN_CMD, qvar_to_str},
      {(void*)&m_chrome_path, SM_CHROME_PATH, qvar_to_str},
      {(void*)&m_subutai_cmd, SM_SUBUTAI_CMD, qvar_to_str},

      // bool
      {(void*)&m_remember_me, SM_REMEMBER_ME, qvar_to_bool},
      {(void*)&m_p2p_autoupdate, SM_P2P_AUTOUPDATE, qvar_to_bool},
      {(void*)&m_rh_autoupdate, SM_RH_AUTOUPDATE, qvar_to_bool},
      {(void*)&m_tray_autoupdate, SM_TRAY_AUTOUPDATE, qvar_to_bool},
      {(void*)&m_use_animations, SM_USE_ANIMATIONS, qvar_to_bool},
      {(void*)&m_autostart, SM_AUTOSTART, qvar_to_bool},

      // uint
      {(void*)&m_p2p_update_freq, SM_P2P_UPDATE_FREQ, qvar_to_int},
      {(void*)&m_rh_update_freq, SM_RH_UPDATE_FREQ, qvar_to_int},
      {(void*)&m_rh_port, SM_RH_PORT, qvar_to_int},
      {(void*)&m_tray_update_freq, SM_TRAY_UPDATE_FREQ, qvar_to_int},
      {(void*)&m_notifications_level, SM_NOTIFICATIONS_LEVEL, qvar_to_int},
      {(void*)&m_preferred_notifications_place,
       SM_PREFERRED_NOTIFICATIONS_PLACE, qvar_to_int},

      // bytearr
      {(void*)&m_password, SM_PASSWORD, qvar_to_byte_arr},

      // QMap<QString, QVariant>
      {(void*)&m_dct_notification_ignore, SM_DCT_NOTIFICATIONS_IGNORE,
       qvar_to_map_string_qvariant},

      // end
      {nullptr, "", nullptr}};

  setting_val_t* tmp_sv = dct_settings_vals;
  for (; tmp_sv->field != nullptr; ++tmp_sv) {
    if (m_settings.value(tmp_sv->val).isNull()) continue;
    tmp_sv->pf_qvar_to_T(m_settings.value(tmp_sv->val), tmp_sv->field);
  }

  bool ok = false;
  if (!m_settings.value(SM_REFRESH_TIME).isNull()) {
    uint32_t timeout = m_settings.value(SM_REFRESH_TIME).toUInt(&ok);
    m_refresh_time_sec = ok ? timeout : def_timeout;
  }

  if (!m_settings.value(SM_NOTIFICATION_DELAY_SEC).isNull()) {
    uint32_t nd = m_settings.value(SM_NOTIFICATION_DELAY_SEC).toUInt(&ok);
    if (ok) set_notification_delay_sec(nd);
  }

  if (m_tray_guid.isEmpty()) {
    m_tray_guid = QUuid::createUuid().toString();
    m_settings.setValue(SM_TRAY_GUID, m_tray_guid);
  }

  // which using
  QString* cmd_which[] = {&m_vboxmanage_path, &m_ssh_keygen_cmd, &m_ssh_path,
                          &m_p2p_path,        &m_terminal_cmd,   nullptr};

  const QString default_values[] = {vboxmanage_command_str(),
                                    ssh_keygen_cmd_path(), ssh_cmd_path(),
                                    default_p2p_path(), default_terminal()};

  QString tmp;
  for (int i = 0; cmd_which[i]; ++i) {
    if (*cmd_which[i] != default_values[i]) continue;
    if (CSystemCallWrapper::which(*cmd_which[i], tmp) != SCWE_SUCCESS) continue;
    *cmd_which[i] = tmp;
  }

  CSystemCallWrapper::set_application_autostart(m_autostart);
  m_autostart = CSystemCallWrapper::application_autostart();  // second check %)
  init_password();
}
////////////////////////////////////////////////////////////////////////////

const QString& CSettingsManager::update_freq_to_str(
    CSettingsManager::update_freq_t fr) {
  static const QString strings[]{
      "1 minute", "5 minutes", "10 minutes", "30 minutes", "1 hour", "3 hour",
      "5 hour",   "Daily",     "Weekly",     "Monthly",    "Never"};
  return strings[fr % UF_LAST];
}
////////////////////////////////////////////////////////////////////////////

uint32_t CSettingsManager::update_freq_to_sec(
    CSettingsManager::update_freq_t fr) {
  static const int min = 60;
  static const int hr = min * 60;
  static const int day = hr * 24;
  static const uint32_t time_sec[]{
      min,    min * 5, min * 10, min * 30, hr, hr * 3,
      hr * 5, day,     day * 7,  day * 28, 0};  // let's say 1 month = 4 week
  return time_sec[fr % UF_LAST];
}
////////////////////////////////////////////////////////////////////////////

static const uint32_t pass_magic = 0xbaedcf3f;
static const uint32_t pass_magic2 = 0xedff019b;

void CSettingsManager::init_password() {
  if (m_password.isEmpty()) {
    //CApplicationLog::Instance()->LogError("Password array is empty");
    qFatal("Password array is empty");
    return;
  }

  do {
    uint32_t* ptr_magic = (uint32_t*)(m_password.data());
    if (m_password.length() <= 8 || ptr_magic[0] != pass_magic ||
        ptr_magic[1] != pass_magic2) {
      break;
    }

    // decrypt
    QByteArray ba = m_password.mid(8);
    int cnt = ba.length();
    char lc = 0;
    QUuid tmp_uuid = QUuid(m_tray_guid);
    uchar* key = tmp_uuid.data4;

    for (int pos = 0; pos < cnt; ++pos) {
      char cc = ba.at(pos);
      ba[pos] = ba.at(pos) ^ lc ^ key[pos % 8];
      lc = cc;
    }

    ba = ba.mid(1);  // remove random byte

    if (ba.length() < 20) {
      //CApplicationLog::Instance()->LogError(
      //    "Decryption error. ba.length() < 20");
      qFatal("Decryption error. ba.length() < 20");
      break;
    }

    QByteArray st_h = ba.left(20);
    ba = ba.mid(20);
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(ba);

    if (hash.result() != st_h) {
      //CApplicationLog::Instance()->LogError(
      //    "Decryption error. hash.result() != stored_hash");
      qFatal("Decryption error. hash.result() != stored_hash");
      break;
    }

    m_password_str = QString(ba);
    return;

  } while (0);

  m_password_str = QString(m_password);
  set_password(m_password_str);
}
////////////////////////////////////////////////////////////////////////////

void CSettingsManager::set_password(const QString& password) {
  QByteArray ba = password.toUtf8();
  QCryptographicHash hash(QCryptographicHash::Sha1);
  hash.addData(ba);
  QByteArray ip = hash.result();
  char rc = (char)(qrand() % 0xff);
  ba = rc + ip + ba;

  int lc = 0;
  QUuid tmp_uuid = QUuid(m_tray_guid);
  uchar* key = tmp_uuid.data4;
  int cnt = ba.length();
  for (int pos = 0; pos < cnt; ++pos) {
    ba[pos] = ba.at(pos) ^ key[pos % 8] ^ lc;
    lc = ba.at(pos);
  }

  QByteArray ra;
  ra.append((const char*)&pass_magic, 4);
  ra.append((const char*)&pass_magic2, 4);
  ra.append(ba);

  m_password = ra;
  m_settings.setValue(SM_PASSWORD, m_password);
}
////////////////////////////////////////////////////////////////////////////

void CSettingsManager::set_logs_storage(const QString& logs_storage) {
  m_logs_storage = logs_storage;
  m_settings.setValue(SM_LOGS_STORAGE, m_logs_storage);
  CApplicationLog::Instance()->SetDirectory(m_logs_storage.toStdString());
}
////////////////////////////////////////////////////////////////////////////

void CSettingsManager::set_p2p_update_freq(int fr) {
  m_p2p_update_freq = (update_freq_t)fr % UF_LAST;
  m_settings.setValue(SM_P2P_UPDATE_FREQ, (int8_t)m_p2p_update_freq);
  update_system::CHubComponentsUpdater::Instance()->set_p2p_update_freq();
}

void CSettingsManager::set_rh_update_freq(int fr) {
  m_rh_update_freq = (update_freq_t)fr % UF_LAST;
  m_settings.setValue(SM_RH_UPDATE_FREQ, (int8_t)m_rh_update_freq);
  update_system::CHubComponentsUpdater::Instance()->set_rh_update_freq();
}

void CSettingsManager::set_tray_update_freq(int fr) {
  m_tray_update_freq = (update_freq_t)fr % UF_LAST;
  m_settings.setValue(SM_TRAY_UPDATE_FREQ, (int8_t)m_tray_update_freq);
  update_system::CHubComponentsUpdater::Instance()->set_tray_update_freq();
}

void CSettingsManager::set_rh_management_freq(int fr) {
  m_rh_management_update_freq = (update_freq_t)fr % UF_LAST;
  m_settings.setValue(SM_RHMANAGEMENT_FREQ,
                      (int8_t)m_rh_management_update_freq);
  update_system::CHubComponentsUpdater::Instance()
      ->set_rh_management_update_freq();
}
////////////////////////////////////////////////////////////////////////////

bool CSettingsManager::is_notification_ignored(const QString& msg) const {
  return (m_dct_notification_ignore.find(msg) !=
          m_dct_notification_ignore.end()) &&
         m_dct_notification_ignore[msg].toBool();
}
////////////////////////////////////////////////////////////////////////////

void CSettingsManager::ignore_notification(const QString& msg) {
  m_dct_notification_ignore[msg] = QVariant(true);
  m_settings.setValue(SM_DCT_NOTIFICATIONS_IGNORE, m_dct_notification_ignore);
  emit notifications_ignored_changed();
}
////////////////////////////////////////////////////////////////////////////

void CSettingsManager::not_ignore_notification(const QString& msg) {
  m_dct_notification_ignore[msg] = QVariant(false);
  m_settings.setValue(SM_DCT_NOTIFICATIONS_IGNORE, m_dct_notification_ignore);
  emit notifications_ignored_changed();
}
////////////////////////////////////////////////////////////////////////////

void CSettingsManager::set_p2p_autoupdate(const bool p2p_autoupdate) {
  m_p2p_autoupdate = p2p_autoupdate;
  m_settings.setValue(SM_P2P_AUTOUPDATE, m_p2p_autoupdate);
  update_system::CHubComponentsUpdater::Instance()->set_p2p_autoupdate();
}

void CSettingsManager::set_rh_autoupdate(const bool rh_autoupdate) {
  m_rh_autoupdate = rh_autoupdate;
  m_settings.setValue(SM_RH_AUTOUPDATE, m_rh_autoupdate);
  update_system::CHubComponentsUpdater::Instance()->set_rh_autoupdate();
}

void CSettingsManager::set_tray_autoupdate(const bool tray_autoupdate) {
  m_tray_autoupdate = tray_autoupdate;
  m_settings.setValue(SM_TRAY_AUTOUPDATE, m_tray_autoupdate);
  update_system::CHubComponentsUpdater::Instance()->set_tray_autoupdate();
}
////////////////////////////////////////////////////////////////////////////

void CSettingsManager::set_rh_management_autoupdate(
    const bool rh_management_autoupdate) {
  m_rh_management_autoupdate = rh_management_autoupdate;
  m_settings.setValue(SM_RHMANAGEMENT_AUTOUPDATE, m_rh_management_autoupdate);
  update_system::CHubComponentsUpdater::Instance()
      ->set_rh_management_autoupdate();
}

void CSettingsManager::set_autostart(const bool autostart) {
  if (m_autostart == autostart) return;
  if (CSystemCallWrapper::set_application_autostart(autostart)) {
    m_autostart = autostart;
    m_settings.setValue(SM_AUTOSTART, m_autostart);
  }
}
  ////////////////////////////////////////////////////////////////////////////

#define SET_FIELD_DEF(f, fn, t)               \
  void CSettingsManager::set_##f(const t f) { \
    m_##f = f;                                \
    m_settings.setValue(fn, m_##f);           \
  }
SET_FIELD_DEF(login, SM_LOGIN, QString&)
SET_FIELD_DEF(remember_me, SM_REMEMBER_ME, bool)
SET_FIELD_DEF(refresh_time_sec, SM_REFRESH_TIME, uint32_t)
SET_FIELD_DEF(p2p_path, SM_P2P_PATH, QString&)
SET_FIELD_DEF(plugin_port, SM_PLUGIN_PORT, uint16_t)
SET_FIELD_DEF(ssh_path, SM_SSH_PATH, QString&)
SET_FIELD_DEF(ssh_user, SM_SSH_USER, QString&)
SET_FIELD_DEF(rh_user, SM_RH_USER, QString&)
SET_FIELD_DEF(rh_pass, SM_RH_PASS, QString&)
SET_FIELD_DEF(rh_host, SM_RH_HOST, QString&)
SET_FIELD_DEF(rh_port, SM_RH_PORT, quint16)
SET_FIELD_DEF(ssh_keys_storage, SM_SSH_KEYS_STORAGE, QString&)
SET_FIELD_DEF(rtm_db_dir, SM_RTM_DB_DIR, QString&)
SET_FIELD_DEF(terminal_cmd, SM_TERMINAL_CMD, QString&)
SET_FIELD_DEF(terminal_arg, SM_TERMINAL_ARG, QString&)
SET_FIELD_DEF(vboxmanage_path, SM_VBOXMANAGE_PATH, QString&)
SET_FIELD_DEF(use_animations, SM_USE_ANIMATIONS, bool)
SET_FIELD_DEF(notifications_level, SM_NOTIFICATIONS_LEVEL, uint32_t)
SET_FIELD_DEF(preferred_notifications_place, SM_PREFERRED_NOTIFICATIONS_PLACE,
              uint32_t)
SET_FIELD_DEF(ssh_keygen_cmd, SM_SSH_KEYGEN_CMD, QString&)
SET_FIELD_DEF(chrome_path, SM_CHROME_PATH, QString&)
#undef SET_FIELD_DEF
