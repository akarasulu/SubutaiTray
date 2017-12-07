#ifndef TRAYWEBSOCKETSERVER_H
#define TRAYWEBSOCKETSERVER_H

#include <QObject>
#include <QList>
#include <QByteArray>

class QWebSocketServer;
class QWebSocket;

/*!
 * \brief This class is used for receiving commands from E2E plugin via Web-socket
 */
class CTrayServer : public QObject  {
  Q_OBJECT

private:

  QWebSocketServer *m_web_socket_server;
  QList<QWebSocket*> m_lst_clients;

  explicit CTrayServer(quint16 port, QObject *parent = Q_NULLPTR);
  ~CTrayServer();

  static void handle_current_user(const QString& msg, QWebSocket* pClient);
  static void handle_ss_ip(const QString& msg, QWebSocket* pClient);  
  static void handle_ssh(const QString& msg, QWebSocket* pClient);
  static void handle_desktop(const QString& msg, QWebSocket* pClient);
  static void handle_wrong_command(const QString& msg, QWebSocket* pClient);

private slots:
  void on_new_connection();
  void process_text_msg(QString msg);
  void process_bin_msg(QByteArray msg);
  void socket_disconnected();
  void ssh_to_container_finished(int result, void* additional_data);

public slots:
public:
  static CTrayServer *Instance(void);
  void Init() const {}
};


#endif // TRAYWEBSOCKETSERVER_H
