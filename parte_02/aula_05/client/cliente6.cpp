//cliente6.cpp -- Licao de casa 2 da fase 5: cliente (computador).
//
// Conecta no servidor6 do Raspberry (porta 3490), recebe os quadros da camera,
// localiza a placa, le o digito e executa o movimento correspondente. Toda a
// logica fica aqui; o Raspberry so obedece aos comandos de 3 bytes:
//   {cmd, velocidade_esq+100, velocidade_dir+100}
//
// COMPILAR (dentro de aula_05):
//   g++ -O3 -fopenmp -std=c++11 client/cliente6.cpp -o client/cliente6 \
//       $(pkg-config --cflags --libs opencv4)
//
// EXECUTAR (dentro de aula_05, para achar assets/):
//   ./client/cliente6 192.168.160.47 [videosaida.avi] [t/c]
//   't' = grava a tela com deteccao + painel (padrao); 'c' = camera crua.
//
// Tecla ESC encerra e manda o servidor parar os motores.
#include <omp.h>
#include <cstdlib>

// Copia completa do projeto.hpp/raspberry.hpp (MNIST, MnistFlann, CLIENT,
// converte, somaAbsDois, dcReject etc.).
#include "../lib/projeto.hpp"

// Deteccao da placa e leitura do digito.
#include "../placa.hpp"

// ---------------------------------------------------------------------------
// Configuracao
// ---------------------------------------------------------------------------

constexpr int NL = 240;   // altura do quadro recebido
constexpr int NC = 320;   // largura do quadro recebido

// Ampliacao SOMENTE da janela (o video gravado continua 440x240).
constexpr int ESCALA_TELA = 2;

// Controle proporcional de centralizacao (igual ao cliente4/aula 4):
// BASE = velocidade media; GANHO = correcao quando a placa esta fora do centro.
constexpr int BASE = 40, GANHO = 40;

// Sentido da curva: +1 como na aula 4; -1 inverte (fiacao dos motores trocada).
constexpr int SENTIDO_CURVA = -1;

// Modelos ampliados: 14 escalas de ~120 px (perto) a ~19 px (longe). Permite
// chegar mais perto antes de parar para ler o digito.
constexpr int   N_ESCALAS_CLIENTE  = 14;
constexpr float ESCALA_MAX_CLIENTE = 0.30f;   // 401 * 0.30 = ~120 px
constexpr float ESCALA_MIN_CLIENTE = 0.0473f; // ~19 px

// Para e le o digito quando a placa tem pelo menos este lado em pixels.
constexpr int LADO_PERTO = 90;

// Velocidades e tempos dos movimentos (calibrar no carrinho real).
constexpr int    V_GIRO     = 70;
constexpr int    V_FRENTE   = 50;
constexpr double T_PARADA   = 0.8;   // espera a inercia acabar
constexpr double T_GIRO_90  = 1.80;  // giro de 90 graus
constexpr double T_GIRO_180 = 3.40;  // giro de 180 graus
constexpr double T_FRENTE   = 2.5;   // passar por baixo da placa

// Leituras antes de decidir (vale o digito mais votado).
constexpr int VOTOS = 3;

// ---------------------------------------------------------------------------
// Maquina de estados (figura 6 do enunciado)
// ---------------------------------------------------------------------------
enum Estado { PROCURA = 0, PARA = 1, LE_DIGITO = 2, MOVE = 3, FIM = 4, ESPERA = 5 };

struct Movimento {
  int esq, dir;       // velocidades -100..100
  double duracao;     // segundos
};

// Tabela digito -> movimento:
//   0,1: para (fim);  2,3: 180 graus;  4,5: em frente;
//   6,7: 90 graus a esquerda;  8,9: 90 graus a direita.
// Rodas em sentidos opostos giram no lugar; iguais andam reto.
Movimento movimento_do_digito(int digito) {
  switch (digito) {
    case 2:  return { -V_GIRO, +V_GIRO, T_GIRO_180 };
    case 3:  return { +V_GIRO, -V_GIRO, T_GIRO_180 };
    case 4: case 5: return { V_FRENTE, V_FRENTE, T_FRENTE };
    case 6: case 7: return { -V_GIRO, +V_GIRO, T_GIRO_90 };
    case 8: case 9: return { +V_GIRO, -V_GIRO, T_GIRO_90 };
  }
  return { 0, 0, 0 };
}

// Centralizacao da aula 4: erro -1 (placa na esquerda) .. +1 (na direita).
// SENTIDO_CURVA inverte a atuacao, caso os motores estejam trocados.
void segue_placa(float cx, int& esq, int& dir) {
  float erro = (cx - NC / 2.0f) / (NC / 2.0f);

  erro *= SENTIDO_CURVA;

  esq = cvRound(BASE + GANHO * erro);
  dir = cvRound(BASE - GANHO * erro);
}

// ---------------------------------------------------------------------------
// Programa principal
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
  if (argc < 2)
    erro("Uso: cliente6 IP [videosaida.avi] [t/c]\n"
         "Exemplo: ./client/cliente6 192.168.160.47 saida.avi t");

  bool gravar = (argc >= 3);
  char modo = (argc >= 4) ? argv[3][0] : 't';

  // Modelos da placa (faixa ampliada) e classificador do MNIST.
  Modelos mod = carrega_modelos("assets/quadrado.png",
                                N_ESCALAS_CLIENTE,
                                ESCALA_MAX_CLIENTE,
                                ESCALA_MIN_CLIENTE);
  ClassificadorDigitos clf(PASTA_MNIST);

  // Conexao com o servidor6 do Raspberry.
  CLIENT client(argv[1]);

  Mat_<COR> a, d, tela;
  Mat_<float> f;
  VideoWriter vo;
  bool mostrar = (getenv("DISPLAY") != nullptr);
  if (mostrar) namedWindow("janela");

  Mat_<COR> painel = monta_painel(Mat_<COR>(), Mat_<uchar>(), -1, NL);

  Estado estado = PROCURA;
  double t0 = 0;
  int votos[10] = {0}, nvotos = 0;
  int digito = -1;
  Movimento mov = {0, 0, 0};

  // Protocolo: '0' executa, 's' encerra; velocidades +100 (0..200).
  // O primeiro comando (parado) so pede o primeiro quadro.
  BYTE cmd[3] = { '0', 100, 100 };
  client.sendBytes(3, cmd);

  while (true) {
    client.receiveImgComp(a);
    if (a.empty()) erro("quadro vazio");

    double agora = timeSinceEpoch();

    int esq = 0, dir = 0;
    bool detectou = false;
    Deteccao det;
    LeituraDigito le;

    // ESTADO 0 (PROCURA): segue a placa; se ela estiver grande, vai para PARA.
    if (estado == PROCURA) {
      converte(a, f);
      det = detecta_placa(f, mod);
      detectou = true;

      if (det.achou) {
        int k = det.melhor;
        if (mod.m[det.escala[k]].cols >= LADO_PERTO) {
          estado = PARA;
          t0 = agora;
        } else {
          float cx = det.pos[k].x + mod.m[det.escala[k]].cols / 2.0f;
          segue_placa(cx, esq, dir);
        }
      }
    }

    // ESTADO 1 (PARA): espera o carrinho parar e vai ler o digito.
    else if (estado == PARA) {
      if (agora - t0 >= T_PARADA) {
        estado = LE_DIGITO;
        nvotos = 0;
        std::fill(votos, votos + 10, 0);
      }
    }

    // ESTADO 2 (LE_DIGITO): acumula VOTOS leituras e decide pelo mais votado.
    else if (estado == LE_DIGITO) {
      converte(a, f);
      det = detecta_placa(f, mod);
      detectou = true;

      le = le_digito(a, mod, det, clf, LADO_PERTO);

      if (le.digito < 0) {
        estado = PROCURA;
      } else {
        painel = monta_painel(le.recorte, le.d14, le.digito, NL);
        votos[le.digito]++;
        nvotos++;

        if (nvotos >= VOTOS) {
          digito = (int)(std::max_element(votos, votos + 10) - votos);

          if (digito <= 1) {
            estado = FIM;
          } else {
            mov = movimento_do_digito(digito);
            estado = MOVE;
            t0 = agora;
          }
        }
      }
    }

    // ESTADO 3 (MOVE): mantem o movimento ate o tempo terminar.
    else if (estado == MOVE) {
      esq = mov.esq;
      dir = mov.dir;

      if (agora - t0 >= mov.duracao) {
        estado = ESPERA;
        t0 = agora;
        esq = dir = 0;
      }
    }

    // ESTADO 5 (ESPERA): espera a inercia e volta a procurar.
    else if (estado == ESPERA) {
      if (agora - t0 >= T_PARADA)
        estado = PROCURA;
    }

    // Monta a tela: quadro + deteccao + painel lateral.
    a.copyTo(d);
    if (detectou) desenha_deteccao(d, mod, det);

    hconcat(d, painel, tela);

    if (mostrar) {
      Mat_<COR> tela_grande;
      resize(tela, tela_grande, Size(), ESCALA_TELA, ESCALA_TELA, INTER_NEAREST);
      imshow("janela", tela_grande);
    }

    // Gravacao opcional ('c' = camera crua; 't' = tela com deteccao).
    if (gravar) {
      Mat saida = (modo == 'c') ? Mat(a) : Mat(tela);
      if (!vo.isOpened())
        vo.open(argv[2], VideoWriter::fourcc('M','J','P','G'), 15, saida.size());
      vo << saida;
    }

    // ESC ou digito 0/1: manda parar e encerra.
    int ch = -1;
    if (mostrar) ch = waitKey(1) & 0xff;

    if (ch == 27 || estado == FIM) {
      cmd[0] = 's';
      cmd[1] = 100;
      cmd[2] = 100;
      client.sendBytes(3, cmd);
      break;
    }

    // Envia o comando do quadro.
    cmd[0] = '0';
    cmd[1] = esq + 100;
    cmd[2] = dir + 100;
    client.sendBytes(3, cmd);
  }

  return 0;
}
