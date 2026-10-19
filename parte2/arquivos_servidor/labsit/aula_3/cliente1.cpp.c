//cliente1.cpp
#include "../projeto.hpp"

int estado = 0;   // 0 = nenhum; 1..9 = botao apertado

void on_mouse(int event, int c, int l, int flags, void* userdata) {
  if (event == EVENT_LBUTTONDOWN) {
    if (l >= 0 && l < 240 && c >= 0 && c < 240)
      estado = (2 - l/80) * 3 + (c/80) + 1;   // 7 8 9 / 4 5 6 / 1 2 3
    else estado = 0;
  } else if (event == EVENT_LBUTTONUP) estado = 0;
}

Mat_<COR> teclado(int est) {
  Mat_<COR> t(240, 240, COR(128,128,128));
  int dx[10] = {0,-1,0, 1,-1,0, 1,-1,0, 1};
  int dy[10] = {0, 1,1, 1, 0,0, 0,-1,-1,-1};
  for (int d=1; d<=9; d++) {
    int r = 2 - (d-1)/3, k = (d-1)%3;
    Point ctr(k*80+40, r*80+40);
    Scalar cor = (d==est) ? Scalar(0,0,255) : Scalar(0,0,120);
    rectangle(t, Rect(k*80, r*80, 80, 80), Scalar(0,0,0), 1);
    if (d==5) circle(t, ctr, 3, cor, -1);
    else arrowedLine(t, ctr - Point(dx[d]*25, dy[d]*25), ctr + Point(dx[d]*25, dy[d]*25), cor, 3);
  }
  return t;
}

int main(int argc, char *argv[]) {
  if (argc < 2) erro("cliente1 IP [videosaida.avi] [t/c]\n");
  bool gravar = (argc >= 3);
  char modo = (argc >= 4) ? argv[3][0] : 't';
  CLIENT client(argv[1]);
  Mat_<COR> img, tela;
  VideoWriter vo;
  namedWindow("janela");
  setMouseCallback("janela", on_mouse);

  BYTE cmd = '0';
  client.sendBytes(1, &cmd);
  while (true) {
    client.receiveImgComp(img);
    hconcat(teclado(estado), img, tela);
    imshow("janela", tela);
    if (gravar) {
      Mat saida = (modo=='c') ? Mat(img) : Mat(tela);
      if (!vo.isOpened())
        vo.open(argv[2], VideoWriter::fourcc('M','J','P','G'), 15, saida.size());
      vo << saida;
    }
    int ch = waitKey(1) & 0xff;
    if (ch == 27) { cmd = 's'; client.sendBytes(1, &cmd); break; }
    cmd = '0' + estado;
    client.sendBytes(1, &cmd);
  }
}
