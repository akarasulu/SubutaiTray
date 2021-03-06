#include "P2PController.h"
#include <QThread>
#include <QNetworkInterface>
#include <QList>
#include <QHostAddress>
#include "Locker.h"
#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include "RestWorker.h"
#include "RhController.h"

//////////////////////////////////////////////////////////////////////////////////////////////////

SynchroPrimitives::CriticalSection P2PConnector::m_cont_critical;
SynchroPrimitives::CriticalSection P2PConnector::m_env_critical;

void P2PConnector::join_swarm(const CEnvironment &env) {
  qInfo() <<
            QString("Trying to join the swarm for [env_name: %1, env_id: %2, swarm_hash: %3]")
             .arg(env.name())
             .arg(env.id())
             .arg(env.hash());

  SwarmConnector *swarm_connector = new SwarmConnector(env);
  connect(swarm_connector, &SwarmConnector::connection_finished, [this, env, swarm_connector](system_call_wrapper_error_t res) {
    if (res == SCWE_SUCCESS) {
      qInfo() << QString("Joined to swarm [env_name: %1, env_id: %2, swarm_hash: %3]")
                 .arg(env.name())
                 .arg(env.id())
                 .arg(env.hash());
    }
    else {
      qCritical()<< QString("Can't join to swarm [env_name: %1, env_id: %2, swarm_hash: %3] Error message: %4")
                    .arg(env.name())
                    .arg(env.id())
                    .arg(env.hash())
                    .arg(CSystemCallWrapper::scwe_error_to_str(res));
    }

    SynchroPrimitives::Locker lock(&P2PConnector::m_env_critical);
    if (res == SCWE_SUCCESS)
      this->connected_envs.insert(env.hash());
    else
      this->connected_envs.erase(env.hash());

    delete swarm_connector;
  });

  swarm_connector->startWork(m_pool);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void P2PConnector::leave_swarm(const QString &hash) {
  qInfo() << QString("This swarm is not found in hub environments. Trying to delete. [swarm_hash: %1]").arg(hash);

  SwarmLeaver *swarm_leaver = new SwarmLeaver(hash);

  connect(swarm_leaver, &SwarmLeaver::connection_finished, [this, hash, swarm_leaver](system_call_wrapper_error_t res) {
    if (res == SCWE_SUCCESS) {
      qInfo() << QString("Left the swarm [swarm_hash: %1]")
                 .arg(hash);
    }
    else {
      qCritical()<< QString("Can't leave the swarm [swarm_hash: %1] Error message: %2")
                    .arg(hash)
                    .arg(CSystemCallWrapper::scwe_error_to_str(res));
    }

    SynchroPrimitives::Locker lock(&P2PConnector::m_env_critical);
    this->connected_envs.erase(hash);

    delete swarm_leaver;
  });

  swarm_leaver->startWork(m_pool);
}


//////////////////////////////////////////////////////////////////////////////////////////////////

void P2PConnector::check_rh(const CEnvironment &env, const CHubContainer &cont) {
  qInfo() <<
            QString("Checking rh status [cont_name: %1, cont_id: %2] [env_name: %3, env_id: %4, swarm_hash: %5]")
             .arg(cont.name())
             .arg(cont.id())
             .arg(env.name())
             .arg(env.id())
             .arg(env.hash());

  RHStatusChecker *rh_checker = new RHStatusChecker(env, cont);

  connect(rh_checker, &RHStatusChecker::connection_finished, [this, env, cont, rh_checker](system_call_wrapper_error_t res) {
    if (res == SCWE_SUCCESS) {
      qInfo() << QString("Successfully handshaked with container [cont_name: %1, cont_id: %2] and env: [env_name: %3, env_id: %4, swarm_hash: %5]")
                 .arg(cont.name())
                 .arg(cont.id())
                 .arg(env.name())
                 .arg(env.id())
                 .arg(env.hash());
    }
    else {
      qInfo() << QString("Can't handshake with container [cont_name: %1, cont_id: %2] and env: [env_name: %3, env_id: %4, swarm_hash: %5]. Error message: %6")
                 .arg(cont.name())
                 .arg(cont.id())
                 .arg(env.name())
                 .arg(env.id())
                 .arg(env.hash())
                 .arg(CSystemCallWrapper::scwe_error_to_str(res));
    }
    SynchroPrimitives::Locker lock(&P2PConnector::m_cont_critical);
    if (res == SCWE_SUCCESS)
      this->connected_conts.insert(std::make_pair(env.hash(), cont.id()));
    else
      this->connected_conts.erase(std::make_pair(env.hash(), cont.id()));

    delete rh_checker;
  });

  rh_checker->startWork(m_pool);
}


void P2PConnector::check_status(const CEnvironment &env) {
  qInfo() <<
            QString("Checking status of environment [env_name: %1, env_id: %2, swarm_hash: %3]")
             .arg(env.name())
             .arg(env.id())
             .arg(env.hash());
  for (CHubContainer cont : env.containers()) {
    check_rh(env, cont);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void P2PConnector::update_status() {
  // find if container is on your machine
  std::vector<CEnvironment> hub_environments = CHubController::Instance().lst_environments();
  QStringList local_containers;
  if (CSystemCallWrapper::is_desktop_peer()) {
    // clear
    local_containers.clear();

    static QString lxc_path("/var/lib/lxc");
    QDir directory(lxc_path);
    QString tmp, hostfile_path;

    // check container directory
    if (directory.exists()) {
      for (QFileInfo info : directory.entryInfoList()) {
        if (info.fileName() == "." || info.fileName() == "..")
          continue;

        tmp = info.fileName().trimmed();
        hostfile_path = lxc_path + QDir::separator() + tmp + QDir::separator() + "rootfs/etc/hostname";

        if (QFileInfo::exists(hostfile_path)) {
          QFile file(hostfile_path);

          if(!file.open(QIODevice::ReadOnly)) {
              qDebug() << "error opening file: " << file.error() << hostfile_path;
              break;
          }

          QTextStream instream(&file);
          QString line = instream.readLine();

          qDebug() << "local container hotname found: " << line;
          local_containers << line;
          file.close();
        } else {
          qInfo() << "not exist container hostname file: "
                  << hostfile_path;
        }
      }

      if (!local_containers.empty()) {
        for (CEnvironment env : hub_environments) {
          for (CHubContainer cont : env.containers()) {
            bool found = false;
            QString peer_id = cont.peer_id();
            QString bazaar_cont = cont.name();

            for (QString local_cont : local_containers) {
              if (local_cont.contains(bazaar_cont)) {
                qInfo() << "matched container "
                        << "bazaar: "
                        << bazaar_cont
                        << "local: "
                        << local_cont;
                found = true;
                break;
              }
            }
            P2PController::Instance().rh_local_tbl[peer_id] = found;
          }
        }
      } else {
        qInfo() << "empty local containers from lxc directory";
      }
    } else {
      qCritical() << "container directory not exist: "
                  << lxc_path;
    }
  }

  if (!CCommons::IsApplicationLaunchable(CSettingsManager::Instance().p2p_path())
      || !CSystemCallWrapper::p2p_daemon_check()) {
    qDebug()<<"p2p path is:"<<CSettingsManager::Instance().p2p_path();
    qCritical() << "P2P is not launchable or p2p daemon is not running.";
    connected_conts.clear();
    connected_envs.clear();
    QTimer::singleShot(30000, this, &P2PConnector::update_status); // wait more, when p2p is not operational
    return;
  }

  qInfo() << "Starting to update connection status";


  QFuture<std::vector<QString>> res_swarm = QtConcurrent::run(CSystemCallWrapper::p2p_show);
  res_swarm.waitForFinished();

  QFuture<std::vector<std::pair<QString, QString>>>  res_interfaces = QtConcurrent::run(CSystemCallWrapper::p2p_show_interfaces);
  res_interfaces.waitForFinished();

  std::vector<QString> joined_swarms = res_swarm.result(); // swarms + interfaces
  std::vector<std::pair<QString, QString>> swarm_interfaces = res_interfaces.result(); // swarms + interfaces



  QStringList already_joined;
  QStringList hub_swarms;
  QStringList lst_interfaces;

  for (auto tt : swarm_interfaces) {
    lst_interfaces << tt.first << " -- " << tt.second << "\n";
  }
  for (auto swarm : joined_swarms){
    already_joined << swarm;
  }
  for (auto env : hub_environments){
    hub_swarms << env.hash();
  }

  qDebug()
      << "Joined swarm: " << already_joined;

  qDebug()
      << "Swarms from hub: " << hub_swarms;

  qDebug()
      << "Swarm Interfaces: " << lst_interfaces;

  // need to clear it everytime
  interface_ids.clear();

  // Setting the interfaces to swarm
  // if an interface found with command `p2p show --interfaces --bind`, then we use that interface
  for (CEnvironment &env : hub_environments) {
    if (!env.healthy())
      continue;
    std::vector<std::pair<QString, QString>>::iterator found_swarm_interface =
        std::find_if(swarm_interfaces.begin(), swarm_interfaces.end(), [&env](const std::pair<QString, QString>& swarm_interface) {
        return env.hash() == swarm_interface.first;
    });

    if (found_swarm_interface != swarm_interfaces.end()
        && env.base_interface_id() == -1) {
      QString interface = found_swarm_interface->second;
      QRegExp regExp("(-?\\d+(?:[\\.,]\\d+(?:e\\d+)?)?)");
      regExp.indexIn(interface);
      int id = regExp.capturedTexts().first().toInt();
      env.set_base_interface_id(id);
      interface_ids[id] = env.hash();
    }
  }

  // if an interface was not found with command, then we will  give the unselected interfaces
  for (CEnvironment &env : hub_environments) {
    if (!env.healthy())
      continue;
    std::vector<std::pair<QString, QString>>::iterator found_swarm_interface =
        std::find_if(swarm_interfaces.begin(), swarm_interfaces.end(), [&env](const std::pair<QString, QString>& swarm_interface) {
        return env.hash() == swarm_interface.first;
    });
    if (found_swarm_interface == swarm_interfaces.end()
        && env.base_interface_id() == -1) {
      int id = get_unselected_interface_id();
      env.set_base_interface_id(id);
      interface_ids[id] = env.hash();
    }
  }

  {
    // WARNING: critical section
    SynchroPrimitives::Locker lock(&P2PConnector::m_env_critical);
    for (CEnvironment env : hub_environments) {
      if (std::find(joined_swarms.begin(), joined_swarms.end(), env.hash()) == joined_swarms.end()) { // environment not found in joined swarm hashes, we need to try to join it
        connected_envs.erase(env.hash());
      }
      else {
        connected_envs.insert(env.hash());
      }
    }
  }

  // joining the swarm
  for (CEnvironment env : hub_environments) {
    if (std::find(joined_swarms.begin(), joined_swarms.end(), env.hash()) == joined_swarms.end()) { // environment not found in joined swarm hashes, we need to try to join it
      if (env.healthy()) {
        join_swarm(env);
      }
    }
  }

  // checking the status of containers, handshaking
  for (CEnvironment env : hub_environments) {
    if (std::find(joined_swarms.begin(), joined_swarms.end(), env.hash()) != joined_swarms.end()) { // environment not found in joined swarm hashes, we need to try to join it
      check_status(env);
    }
  }

  // checking deleted environments
  for (QString hash : joined_swarms) {
    auto found_env = std::find_if(hub_environments.begin(), hub_environments.end(), [&hash](const CEnvironment& env) {
      return env.hash() == hash;
    });
    if (found_env == hub_environments.end()) {
      leave_swarm(hash);
    }
  }
  QTimer::singleShot(15000, this, &P2PConnector::update_status);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

P2PController::P2PController() {
   qDebug() << "P2P controller is initialized";

   m_pool = new QThreadPool(this);
   m_pool->setMaxThreadCount(1);
   connector = new P2PConnector;
   connector->set_pool(m_pool);

   QTimer::singleShot(5000, connector, SLOT(update_status()));
}


void P2PController::update_p2p_status() {
  CRestWorker::Instance()->update_p2p_status();
}

void P2PController::p2p_status_updated_sl(std::vector<CP2PInstance> new_p2p_instances,
                                          int http_code,
                                          int err_code,
                                          int network_error){
  UNUSED_ARG(http_code);
  if (err_code || network_error) {
    qCritical(
        "Refresh p2p status failed. Err_code : %d, Net_err : %d", err_code,
        network_error);
    return;
  }

  m_p2p_instances = new_p2p_instances;
}

P2PController::P2P_CONNECTION_STATUS
P2PController::is_ready(const CEnvironment&env, const CHubContainer &cont) {
  if(!connector->env_connected(env.hash()))
    return CANT_JOIN_SWARM;
  else
  if(!connector->cont_connected(env.hash(), cont.id()))
    return CANT_CONNECT_CONT;
  else
    return CONNECTION_SUCCESS;
}

P2PController::P2P_CONNECTION_STATUS
P2PController::is_swarm_connected(const CEnvironment&env) {
  if(!connector->env_connected(env.hash()))
    return CANT_JOIN_SWARM;
  else
    return CONNECTION_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

QString P2PController::p2p_connection_status_to_str(P2P_CONNECTION_STATUS status) {
  static QString str [] = {"Successfully connected",
                           "Can't join to swarm with environment",
                           "Handshaking with container"
                          };
  return str[static_cast<size_t>(status)];
}

ssh_desktop_launch_error_t P2PController::is_ready_sdle(const CEnvironment& env, const CHubContainer& cont) {
  P2P_CONNECTION_STATUS ret = is_ready(env, cont);
  static ssh_desktop_launch_error_t res[] = {
    SDLE_SUCCESS,
    SDLE_JOIN_TO_SWARM_FAILED,
    SDLE_CONT_NOT_READY,
  };
  return res[static_cast<size_t>(ret)];
}

//p2p status updater
void P2PStatus_checker::update_status(){
    qDebug()
            <<"updating p2p status";
    if(!CCommons::IsApplicationLaunchable(CSettingsManager::Instance().p2p_path())) {
      emit p2p_status(P2P_FAIL);
      QTimer::singleShot(5*1000, this, &P2PStatus_checker::update_status);
    } else {
      if (!CSystemCallWrapper::p2p_daemon_check()) {
        emit p2p_status(P2P_READY);
        QTimer::singleShot(5*1000, this, &P2PStatus_checker::update_status);
      } else {
        emit p2p_status(P2P_RUNNING);
        QTimer::singleShot(30*1000, this, &P2PStatus_checker::update_status);
      }
    }
}
