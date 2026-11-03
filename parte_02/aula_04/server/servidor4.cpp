//servidor4.cpp
//Raspberry$ compila servidor4 -ocv -w
#include <wiringPi.h>
#include <softPwm.h>
#include "../projeto.hpp"

const int ESQ_F=0, ESQ_T=1, DIR_F=2, DIR_T=3;
const double K_ESQ = 1.0, K_DIR = 1.0;   // mesmos valores calibrados no andametro.cpp

void motor(int pf, int pt, int v) {
  if (v >= 0) { softPwmWrite(pf, v); softPwmWrite(pt, 0); }
  else        { softPwmWrite(pf, 0); softPwmWrite(pt, -v); }
}
// esq e dir: velocidade de -100 a 100 (negativo = re)
void motores(int esq, int dir) {
  motor(ESQ_F, ESQ_T, (int)(esq*K_ESQ));
  motor(DIR_F, DIR_T, (int)(dir*K_DIR));
}

int main() {
  wiringPiSetup();
  int pinos[4] = {ESQ_F, ESQ_T, DIR_F, DIR_T};
  for (int i=0; i<4; i++) if (softPwmCreate(pinos[i], 0, 100)) erro("softPwmCreate");
  SERVER server;
  server.waitConnection();
  VideoCapture w(0);
  if (!w.isOpened()) erro("Camera nao abriu");
  w.set(CAP_PROP_FRAME_WIDTH, 320);
  w.set(CAP_PROP_FRAME_HEIGHT, 240);
  Mat_<COR> img;
  BYTE cmd[3];
  server.receiveBytes(3, cmd);               // mensagem inicial
  w >> img; resize(img, img, Size(320,240));
  server.sendImgComp(img);                   // primeiro quadro
  while (true) {
    server.receiveBytes(3, cmd);             // cmd, esq+100, dir+100
    if (cmd[0] == 's') break;
    motores(cmd[1] - 100, cmd[2] - 100);
    w >> img; resize(img, img, Size(320,240));
    server.sendImgComp(img);
  }
  motores(0, 0);                             // para o carrinho ao sair
  delay(100);
  for (int i=0; i<4; i++) softPwmStop(pinos[i]);
}
