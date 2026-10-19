//camclient2.cpp
#include "../projeto.hpp"
int main(int argc, char *argv[]) {
  if (argc != 2) erro("camclient2 ServerIP\n");
  CLIENT client(argv[1]);
  Mat_<COR> img;
  BYTE cmd = '0';
  int i = 0;
  double t1 = timeSinceEpoch();
  client.sendBytes(1, &cmd);          // pede o primeiro quadro
  while (true) {
    client.receiveImgComp(img);
    i++;
    imshow("janela", img);
    int ch = waitKey(1) & 0xff;
    cmd = (ch == 27) ? 's' : '0';     // ESC = 27
    client.sendBytes(1, &cmd);        // confirmacao
    if (cmd == 's') break;
  }
  double t = timeSinceEpoch() - t1;
  printf("Quadros=%d tempo=%8.2fs fps=%8.2f\n", i, t, i/t);
}
