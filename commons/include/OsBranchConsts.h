#ifndef OSBRANCHCONSTS_H
#define OSBRANCHCONSTS_H

#include <QString>

enum branch_t {
  BT_MASTER = 0,
  BT_PROD = 1,
  BT_DEV = 2
};

enum os_t {
  OS_LINUX = 0,
  OS_WIN = 1,
  OS_MAC = 2,
  OS_MAC_LIN = 3
};

template<branch_t v> struct Branch2Type {
  enum {val = v};
};

template<os_t v> struct Os2Type {
  enum {val = v};
};

template<int v> struct Int2Type {
  enum {val = v};
};
////////////////////////////////////////////////////////////////////////////

const QString& tray_kurjun_file_name();
const QString& p2p_kurjun_file_name();
const QString& p2p_dht_arg();

const QString& hub_post_url();
const QString& hub_register_url();
const QString& hub_get_url();
const QString& hub_health_url();
const QString& hub_gorjun_url();
const QString& hub_billing_url();

const char ** ssdp_rh_search_target_arr();

const QString& default_p2p_path();
const QString& default_terminal();
const QString& default_term_arg();

const QString& vboxmanage_command_str();
const QString& hub_site();
const QString& ssh_keygen_cmd_path();
const QString& ssh_cmd_path();

const QString& which_cmd();
const QString& default_chrome_path();

const QString& subutai_command();
const QString& default_ip_addr_cmd();
const QString& snap_p2p_path();

const QString& p2p_package_url();

#endif // OSBRANCHCONSTS_H
