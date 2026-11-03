//servidor1.cpp
#include "../projeto.hpp"
int main() {
  SERVER server;
  server.waitConnection();
  VideoCapture w(0);
  if (!w.isOpened()) erro("Camera nao abriu");
  w.set(CAP_PROP_FRAME_WIDTH, 320);
  w.set(CAP_PROP_FRAME_HEIGHT, 240);
  Mat_<COR> img;
  BYTE cmd;
  server.receiveBytes(1, &cmd);              // mensagem inicial
  w >> img; resize(img, img, Size(320,240));
  server.sendImgComp(img);                   // primeiro quadro, sem texto
  while (true) {
    server.receiveBytes(1, &cmd);
    if (cmd == 's') break;
    w >> img; resize(img, img, Size(320,240));
    putText(img, string(1, cmd), Point(10,40), 0, 1.5, Scalar(255,255,255), 2);
    server.sendImgComp(img);
  }
}
