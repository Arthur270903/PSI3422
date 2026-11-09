//servidor6.cpp -- Licao de casa 2 da fase 5 (vale 5): servidor (Raspberry).
//
// O QUE O PROGRAMA FAZ:
//   - configura os 4 pinos do driver dos motores com softPwm;
//   - espera a conexao do cliente6 (computador) na porta 3490;
//   - captura um quadro da camera, comprime em JPEG e envia ao cliente;
//   - recebe do cliente um comando de 3 bytes:
//         cmd[0] = '0' para executar / 's' para encerrar;
//         cmd[1] = velocidade da roda esquerda + 100 (0..200);
//         cmd[2] = velocidade da roda direita  + 100 (0..200);
//   - aplica as velocidades nos motores e devolve o proximo quadro;
//   - ao terminar (fim normal, erro, Ctrl+C ou queda da conexao), desliga os
//     motores para o carrinho nao sair andando sozinho.
//
// COMPILAR NO RASPBERRY (dentro da pasta que contem este arquivo):
//   g++ -O3 server6.cpp -o server6 $(pkg-config --cflags --libs opencv) \
//       -lwiringPi -lpthread
//
//   No Raspberry a OpenCV instalada e' a 3.2, por isso o pkg-config se chama
//   "opencv" (e nao "opencv4", que e' usado no computador).
//   Se preferir, use o script "compila" do professor: compila servidor6 -ocv -w
//
// EXECUTAR NO RASPBERRY:
//   ./server6
//   (o cliente6 no computador deve conectar no IP do Raspberry, porta 3490)
#include <wiringPi.h>
#include <softPwm.h>
#include <signal.h>
#include "projeto.hpp"

// ---------------------------------------------------------------------------
// Pinos do driver dos motores (numeracao wiringPi).
//
//   ESQ_F = pino do motor esquerdo, para frente;
//   ESQ_T = pino do motor esquerdo, para tras;
//   DIR_F = pino do motor direito, para frente;
//   DIR_T = pino do motor direito, para tras.
// ---------------------------------------------------------------------------
const int ESQ_F = 0, ESQ_T = 1, DIR_F = 2, DIR_T = 3;
const int PINOS[4] = {ESQ_F, ESQ_T, DIR_F, DIR_T};

// Fatores de calibracao de cada lado. O motor ESQUERDO do nosso carrinho e'
// um pouco mais fraco, por isso K_ESQ = 1.20 (aumenta ~20% o comando dele).
// Ajuste fino, se necessario: 1.10 / 1.15 / 1.25 / 1.30.
const double K_ESQ = 1.20, K_DIR = 1.0;

// Quantos quadros descartar antes de pegar o quadro mais recente. A camera
// pode manter alguns quadros antigos no buffer; descartar evita enviar um
// quadro "atrasado" para o cliente.
constexpr int N_GRAB = 2;

// ---------------------------------------------------------------------------
// Aciona UM motor.
//
//   pf = pino "para frente"; pt = pino "para tras"; v = velocidade -100..100.
//   v >= 0: liga o pino de frente com v e o de tras com 0;
//   v <  0: liga o pino de tras com -v e o de frente com 0.
// ---------------------------------------------------------------------------
void motor(int pf, int pt, int v) {
  if (v >= 0) {
    softPwmWrite(pf, v);
    softPwmWrite(pt, 0);
  } else {
    softPwmWrite(pf, 0);
    softPwmWrite(pt, -v);
  }
}

// ---------------------------------------------------------------------------
// Aciona os dois motores.
//
//   esq, dir = velocidades -100..100 (negativo = re).
//   Os fatores K_ESQ/K_DIR corrigem diferencas mecanicas entre os motores.
// ---------------------------------------------------------------------------
void motores(int esq, int dir) {
  // Aplica o fator de calibracao e limita em -100..100. O limite e' necessario
  // porque K_ESQ > 1 pode gerar valores fora da faixa do softPwm (0..100).
  int v_esq = (int)(esq * K_ESQ);
  int v_dir = (int)(dir * K_DIR);
  if (v_esq >  100) v_esq =  100;
  if (v_esq < -100) v_esq = -100;
  if (v_dir >  100) v_dir =  100;
  if (v_dir < -100) v_dir = -100;

  motor(ESQ_F, ESQ_T, v_esq);
  motor(DIR_F, DIR_T, v_dir);
}

// ---------------------------------------------------------------------------
// Desliga os motores e encerra o softPwm.
//
// Esta funcao e' registrada com atexit(). Assim, ela roda sempre que o
// programa termina: no fim normal, em erro() ou quando o Ctrl+C chama exit().
// Isso evita o problema de o programa terminar e os motores continuarem
// girando.
// ---------------------------------------------------------------------------
void desliga() {
  motores(0, 0);       // manda velocidade zero para os dois motores
  delay(100);          // espera 100 ms para o driver obedecer

  // Para o PWM de cada pino.
  for (int i = 0; i < 4; i++) softPwmStop(PINOS[i]);
}

// ---------------------------------------------------------------------------
// Tratador do Ctrl+C (SIGINT).
//
// Em vez de simplesmente morrer, chama exit(0). O exit() executa as funcoes
// registradas em atexit(), inclusive desliga(). Assim os motores param.
// ---------------------------------------------------------------------------
void ao_receber_sinal(int) {
  exit(0);
}

// ---------------------------------------------------------------------------
// Captura o quadro MAIS RECENTE da camera.
//
// O buffer da camera pode ter quadros velhos. Fazemos N_GRAB grabs (sem
// converter) para descarta-los e depois lemos o quadro seguinte com ">>".
// Por fim, redimensiona para 320x240, o mesmo tamanho usado pelo cliente.
// ---------------------------------------------------------------------------
void captura(VideoCapture& w, Mat_<COR>& img) {
  for (int i = 0; i < N_GRAB; i++) w.grab();  // descarta quadros antigos
  w >> img;                                   // pega o quadro atual
  resize(img, img, Size(320, 240));           // padroniza o tamanho
}

// ---------------------------------------------------------------------------
// Programa principal
// ---------------------------------------------------------------------------
int main() {
  // -------------------------------------------------------------------------
  // 1) Configuracao do wiringPi e do softPwm.
  //
  // wiringPiSetup() usa a numeracao wiringPi dos pinos.
  // softPwmCreate(pino, valorInicial, faixa): cria um PWM de 0..100.
  // -------------------------------------------------------------------------
  wiringPiSetup();
  for (int i = 0; i < 4; i++)
    if (softPwmCreate(PINOS[i], 0, 100)) erro("softPwmCreate");

  // Garante que os motores sejam desligados em qualquer saida do programa.
  atexit(desliga);

  // Ctrl+C tambem passa por exit(), que executa o atexit(desliga).
  signal(SIGINT, ao_receber_sinal);

  // -------------------------------------------------------------------------
  // 2) Espera a conexao do cliente6 (computador).
  // -------------------------------------------------------------------------
  SERVER server;
  server.waitConnection();

  // -------------------------------------------------------------------------
  // 3) Abre a camera.
  //
  // VideoCapture(0) usa a camera por padrao (/dev/video0). No Raspberry do
  // laboratorio e' a camera USB Logitech.
  // -------------------------------------------------------------------------
  VideoCapture w(0);
  if (!w.isOpened()) erro("Camera nao abriu");

  // Pede 320x240 para a camera. Se a camera nao aceitar, o resize do captura()
  // garante o tamanho final.
  w.set(CAP_PROP_FRAME_WIDTH, 320);
  w.set(CAP_PROP_FRAME_HEIGHT, 240);

  Mat_<COR> img;       // quadro colorido
  BYTE cmd[3];         // comando recebido do cliente

  // -------------------------------------------------------------------------
  // 4) Handshake inicial.
  //
  // O cliente manda 3 bytes (inicialmente parado) e o servidor responde com o
  // primeiro quadro. Isso mantem o protocolo simples: 1 comando -> 1 quadro.
  // -------------------------------------------------------------------------
  server.receiveBytes(3, cmd);  // mensagem inicial
  captura(w, img);
  server.sendImgComp(img);      // primeiro quadro (JPEG)

  // -------------------------------------------------------------------------
  // 5) Laco principal.
  // -------------------------------------------------------------------------
  while (true) {
    // Recebe o comando do computador.
    server.receiveBytes(3, cmd);

    // 's' = encerrar. Sai do laco; o atexit(desliga) para os motores.
    if (cmd[0] == 's') break;

    // Converte as velocidades de 0..200 para -100..100 e aciona os motores.
    motores(cmd[1] - 100, cmd[2] - 100);

    // Captura e envia o proximo quadro para o cliente.
    captura(w, img);
    server.sendImgComp(img);
  }
}
