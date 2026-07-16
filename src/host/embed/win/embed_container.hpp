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

  void clearForeignWindow();
  void setForeignWindow(quintptr wid);
  [[nodiscard]] quintptr foreignWindow() const { return foreignWid_; }

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;

private:
  void syncForeignGeometry();
  void applyEmbed();

  quintptr foreignWid_ = 0;
};

}  // namespace mps::host
