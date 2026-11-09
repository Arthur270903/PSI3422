// RASCUNHO ORIGINAL SALVO PELA IA.
// A versao corrigida, comentada e com os caminhos certos esta em:
//   client/cliente6.cpp
// (este arquivo foi mantido apenas como referencia; ele nao compila como esta,
//  pois usa o raspberry.hpp curto e o include "placa.hpp" sem "../")
//cliente6.cpp -- g++ -O3 -fopenmp cliente6.cpp -o cliente6 $(pkg-config --cflags --libs opencv4)
//Uso: ./cliente6 192.168.15.3 [videosaida.avi] [t/c]   (assets/ na pasta atual)
#include <omp.h>
#include "../projeto.hpp"
#include "placa.hpp"

constexpr int NL = 240, NC = 320;
constexpr int BASE = 40, GANHO = 40;                 // seguir a placa (igual ao cliente4)
constexpr int V_GIRO = 60, V_FRENTE = 50;            // CALIBRE
constexpr double T_PARADA   = 0.8;                   // s para o carrinho parar (inercia) -- CALIBRE
constexpr double T_GIRO_90  = 0.45;                  // s para girar 90 graus  -- CALIBRE
constexpr double T_GIRO_180 = 0.9;                   // s para girar 180 graus -- CALIBRE
constexpr double T_FRENTE   = 1.5;                   // s para passar por baixo da placa -- CALIBRE
constexpr int VOTOS = 3;                             // leituras do digito; vale a mais frequente

// Estados da maquina (numeracao da figura 6 do enunciado)
enum Estado { PROCURA = 0, PARA = 1, LE_DIGITO = 2, MOVE = 3, FIM = 4, ESPERA = 5 };

struct Movimento { int esq, dir; double duracao; };  // velocidades das rodas (-100..100) e tempo (s)

// Tabela digito -> movimento. Girar no lugar: rodas em sentidos opostos.
Movimento movimento_do_digito(int digito) {
  switch (digito) {
    case 2:  return { -V_GIRO, +V_GIRO, T_GIRO_180 };    // 180 graus a esquerda
    case 3:  return { +V_GIRO, -V_GIRO, T_GIRO_180 };    // 180 graus a direita
    case 4: case 5: return { V_FRENTE, V_FRENTE, T_FRENTE };  // passa por baixo e segue
    case 6: case 7: return { -V_GIRO, +V_GIRO, T_GIRO_90 };   // 90 graus a esquerda
    case 8: case 9: return { +V_GIRO, -V_GIRO, T_GIRO_90 };   // 90 graus a direita
  }
  return { 0, 0, 0 };
}

// Controle proporcional (so a posicao x da placa), igual ao cliente4.
// dirx = deslocamento da placa em % (negativo = esquerda).
void segue_placa(float cx, int& esq, int& dir, int& dirx) {
  float e = (cx - NC / 2.0f) / (NC / 2.0f);          // -1 (esquerda) .. +1 (direita)
  dirx = cvRound(100 * e);
  esq = std::max(-100, std::min(100, cvRound(BASE + GANHO * e)));
  dir = std::max(-100, std::min(100, cvRound(BASE - GANHO * e)));
}

int main(int argc, char* argv[]) {
  if (argc < 2) erro("cliente6 IP [videosaida.avi] [t/c]\n");
  bool gravar = (argc >= 3);
  char modo = (argc >= 4) ? argv[3][0] : 't';

  Modelos mod = carrega_modelos("assets/quadrado.png");
  ClassificadorDigitos clf(PASTA_MNIST);
  CLIENT client(argv[1]);

  Mat_<COR> a, d, tela;
  Mat_<float> f;
  VideoWriter vo;
  namedWindow("janela");
  Mat_<COR> painel = monta_painel(Mat_<COR>(), Mat_<uchar>(), -1, NL);   // ultimo painel de digito

  Estado estado = PROCURA;
  double t0 = 0;                   // cronometro do estado atual
  int votos[10] = {0}, nvotos = 0; // contagem de leituras do digito
  int digito = -1;                 // ultimo digito decidido
  Movimento mov = {0, 0, 0};

  BYTE cmd[3] = {'0', 100, 100};   // protocolo igual ao cliente4: cmd, esq+100, dir+100
  client.sendBytes(3, cmd);        // pede o primeiro quadro (parado)

  while (true) {
    client.receiveImgComp(a);
    if (a.empty()) erro("quadro vazio");
    double agora = timeSinceEpoch();
    int esq = 0, dir = 0, dirx = 0;            // por padrao: parado
    bool detectou = false;
    Deteccao det; LeituraDigito le;

    if (estado == PROCURA) {                   // ESTADO 0: detecta e segue a placa
      converte(a, f); det = detecta_placa(f, mod); detectou = true;
      if (det.achou) {
        int k = det.melhor;
        if (det.escala[k] <= ESCALA_PERTO) { estado = PARA; t0 = agora; }   // placa grande: para
        else segue_placa(det.pos[k].x + mod.m[det.escala[k]].cols / 2.0f, esq, dir, dirx);
      }                                        // sem placa: continua parado
    } else if (estado == PARA) {               // ESTADO 1: espera o carrinho parar
      if (agora - t0 >= T_PARADA) { estado = LE_DIGITO; nvotos = 0; std::fill(votos, votos + 10, 0); }
    } else if (estado == LE_DIGITO) {          // ESTADO 2: le o digito com o carrinho parado
      converte(a, f); det = detecta_placa(f, mod); detectou = true;
      le = le_digito(a, mod, det, clf);
      if (le.digito < 0) {                     // sem placa/sem leitura: volta ao estado 0
        estado = PROCURA;
      } else {
        painel = monta_painel(le.recorte, le.d14, le.digito, NL);
        votos[le.digito]++; nvotos++;
        if (nvotos >= VOTOS) {                 // decide pelo digito mais votado
          digito = (int)(std::max_element(votos, votos + 10) - votos);
          if (digito <= 1) estado = FIM;       // 0 ou 1: terminar
          else { mov = movimento_do_digito(digito); estado = MOVE; t0 = agora; }
        }
      }
    } else if (estado == MOVE) {               // ESTADO 3: executa o movimento por TEMPO
      esq = mov.esq; dir = mov.dir;
      if (agora - t0 >= mov.duracao) { estado = ESPERA; t0 = agora; esq = dir = 0; }
    } else if (estado == ESPERA) {             // ESTADO 5: espera o giro cessar
      if (agora - t0 >= T_PARADA) estado = PROCURA;
    }                                          // ESTADO 4 (FIM) e' tratado abaixo

    // ---- tela de depuracao ----
    a.copyTo(d);
    if (detectou) desenha_deteccao(d, mod, det);
    char hud[96];
    snprintf(hud, sizeof hud, "estado=%d dirx=%d digito=%d conta=%d", (int)estado, dirx, digito, nvotos);
    putText(d, hud, Point(5, NL - 8), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0,0,255), 1);
    hconcat(d, painel, tela);
    imshow("janela", tela);
    if (gravar) {
      Mat saida = (modo == 'c') ? Mat(a) : Mat(tela);        // c = so camera; t = tela
      if (!vo.isOpened()) vo.open(argv[2], VideoWriter::fourcc('M','J','P','G'), 15, saida.size());
      vo << saida;
    }

    int ch = waitKey(1) & 0xff;
    if (ch == 27 || estado == FIM) {           // ESC ou estado 4: encerra cliente e servidor
      cmd[0] = 's'; cmd[1] = 100; cmd[2] = 100;
      client.sendBytes(3, cmd);
      break;
    }
    cmd[0] = '0'; cmd[1] = esq + 100; cmd[2] = dir + 100;
    client.sendBytes(3, cmd);                  // envia comando; o servidor responde com novo quadro
  }
}