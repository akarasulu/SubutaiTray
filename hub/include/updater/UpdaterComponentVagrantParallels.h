#ifndef UPDATERCOMPONENTVAGRANTPARALLELS_H
#define UPDATERCOMPONENTVAGRANTPARALLELS_H
#include "updater/IUpdaterComponent.h"

namespace update_system {
/**
 * @brief The CUpdaterComponentVAGRANT_PARALLELS class implements
 * IUpdaterComponent.
 */
class CUpdaterComponentVAGRANT_PARALLELS : public IUpdaterComponent {
  // IUpdaterComponent interface
 public:
  CUpdaterComponentVAGRANT_PARALLELS();
  virtual ~CUpdaterComponentVAGRANT_PARALLELS();

  // IUpdaterComponent interface
 protected:
  virtual bool update_available_internal();
  virtual chue_t update_internal();
  virtual void update_post_action(bool success);
  virtual chue_t install_internal();
  virtual chue_t uninstall_internal();
  virtual void install_post_internal(bool success);
  virtual void uninstall_post_internal(bool success);
};
}  // namespace update_system
#endif // UPDATERCOMPONENTVAGRANTPARALLELS_H
