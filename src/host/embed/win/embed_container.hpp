#pragma once

#include <QWidget>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace mps::host {

/// Native container that hosts a foreign HWND via SetParent (Windows).
class EmbedContainer final : public QWidget {
  Q_OBJECT
public:
  explicit EmbedContainer(QWidget* parent = nullptr);

  /// Detach and optionally hide the foreign window, then forget it.
  void clearForeignWindow(bool hide = true);
  /// Stop tracking without SetParent(null)/Hide — next host will reparent.
  void releaseForeignWindow();
  void setForeignWindow(quintptr wid);
  /// Force geometry + show after shell move / reattach.
  void resyncForeignWindow();
  [[nodiscard]] quintptr foreignWindow() const { return foreignWid_; }

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;

private:
  void syncForeignGeometry();
  void applyEmbed();
  [[nodiscard]] bool foreignAlive() const;

  quintptr foreignWid_ = 0;
};

}  // namespace mps::host
