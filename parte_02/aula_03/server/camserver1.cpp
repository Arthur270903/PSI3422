//camserver1.cpp
#include "../projeto.hpp"
int main() {
  SERVER server;
  server.waitConnection();
  VideoCapture w(0);
  if (!w.isOpened()) erro("Camera nao abriu");
  w.set(CAP_PROP_FRAME_WIDTH, 640);
  w.set(CAP_PROP_FRAME_HEIGHT, 480);
  Mat_<COR> img;
  BYTE cmd;
  while (true) {
    server.receiveBytes(1, &cmd);   // '0' = quero quadro, 's' = sair
    if (cmd == 's') break;
    w >> img;
    server.sendImg(img);
  }
}
