#include "embed_container.hpp"

#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>

namespace mps::host {

EmbedContainer::EmbedContainer(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_DontCreateNativeAncestors, false);
  setMinimumSize(200, 150);
  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);
}

void EmbedContainer::clearForeignWindow() {
#ifdef Q_OS_WIN
  if (foreignWid_) {
    const HWND child = reinterpret_cast<HWND>(foreignWid_);
    SetParent(child, nullptr);
    ShowWindow(child, SW_HIDE);
  }
#endif
  foreignWid_ = 0;
}

void EmbedContainer::setForeignWindow(quintptr wid) {
  if (foreignWid_ == wid) {
    syncForeignGeometry();
    return;
  }
  clearForeignWindow();
  foreignWid_ = wid;
  applyEmbed();
}

void EmbedContainer::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  syncForeignGeometry();
}

void EmbedContainer::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  applyEmbed();
}

void EmbedContainer::applyEmbed() {
#ifdef Q_OS_WIN
  if (!foreignWid_) {
    return;
  }
  winId();  // ensure native handle
  const HWND host = reinterpret_cast<HWND>(winId());
  const HWND child = reinterpret_cast<HWND>(foreignWid_);
  LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
  style |= WS_CHILD;
  style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
             WS_SYSMENU);
  SetWindowLongPtrW(child, GWL_STYLE, style);
  SetParent(child, host);
  ShowWindow(child, SW_SHOW);
  syncForeignGeometry();
#else
  Q_UNUSED(foreignWid_);
#endif
}

void EmbedContainer::syncForeignGeometry() {
#ifdef Q_OS_WIN
  if (!foreignWid_) {
    return;
  }
  const HWND child = reinterpret_cast<HWND>(foreignWid_);
  MoveWindow(child, 0, 0, width(), height(), TRUE);
#endif
}

}  // namespace mps::host
