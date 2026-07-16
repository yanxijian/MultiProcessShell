#include "embed_container.hpp"

#include <QResizeEvent>
#include <QShowEvent>

namespace mps::host {

EmbedContainer::EmbedContainer(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_DontCreateNativeAncestors, false);
  setMinimumSize(200, 150);
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
             WS_SYSMENU | WS_BORDER | WS_DLGFRAME);
  SetWindowLongPtrW(child, GWL_STYLE, style);

  LONG_PTR ex = GetWindowLongPtrW(child, GWL_EXSTYLE);
  ex &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME | WS_EX_STATICEDGE |
          WS_EX_TOOLWINDOW);
  SetWindowLongPtrW(child, GWL_EXSTYLE, ex);

  SetParent(child, host);
  SetWindowPos(child, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
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
  winId();
  const HWND host = reinterpret_cast<HWND>(winId());
  const HWND child = reinterpret_cast<HWND>(foreignWid_);
  RECT rc{};
  GetClientRect(host, &rc);
  const int w = qMax(1, static_cast<int>(rc.right - rc.left));
  const int h = qMax(1, static_cast<int>(rc.bottom - rc.top));
  SetWindowPos(child, nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);
#endif
}

}  // namespace mps::host
