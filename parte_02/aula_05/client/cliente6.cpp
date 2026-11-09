//cliente6.cpp -- Licao de casa 2 da fase 5 (vale 5): cliente (computador).
//
// O QUE O PROGRAMA FAZ:
//   - conecta no servidor6 que roda no Raspberry (porta 3490);
//   - recebe os quadros da camera enviados pelo Raspberry;
//   - localiza a placa e, quando ela esta grande (escala 0 ou 1), le o digito;
//   - usa uma MAQUINA DE ESTADOS FINITA (figura 6 do enunciado) para decidir
//     quando seguir a placa, parar, ler o digito, executar o movimento e voltar
//     a procurar a proxima placa;
//   - envia ao Raspberry comandos de 3 bytes: {cmd, velocidade_esq+100,
//     velocidade_dir+100}. O Raspberry apenas executa o comando; toda a logica
//     fica aqui no computador.
//
// COMPILAR (no computador, dentro da pasta aula_05):
//   g++ -O3 -fopenmp -std=c++11 client/cliente6.cpp -o client/cliente6 \
//       $(pkg-config --cflags --libs opencv4)
//
// EXECUTAR (dentro da pasta aula_05, para achar assets/):
//   ./client/cliente6 192.168.160.47 [videosaida.avi] [t/c]
//
//   - o primeiro argumento e' o IP do Raspberry;
//   - o segundo (opcional) e' o video de saida;
//   - o terceiro (opcional) e' o modo do video:
//       't' = grava a tela de depuracao (quadro + painel + textos) [padrao];
//       'c' = grava somente a imagem crua da camera.
//
// LIMIAR DE DETECCAO (opcional, via variavel de ambiente):
//   O padrao e' NCC_MIN=0.55, igual ao fase3p da aula 4. O NCC do melhor
//   candidato aparece no canto superior esquerdo da janela ("NCC: xx%").
//   Se a placa aparece mas o NCC fica logo abaixo de 55%, pode-se afrouxar:
//     NCC_MIN=0.50 ./client/cliente6 192.168.160.47
//
// JANELA: a visualizacao e' ampliada 2x (880x480); o video gravado continua
// em 440x240.
//
// TECLA ESC: encerra o cliente e manda o servidor parar os motores.
#include <omp.h>
#include <cstdlib>

// Inclui a versao COMPLETA do projeto.hpp/raspberry.hpp (com MNIST, MnistFlann,
// CLIENT, converte, somaAbsDois, dcReject etc.). Ela esta na copia lib/; os
// arquivos originais da aula_05 e da aula_04 nao foram alterados.
#include "../lib/projeto.hpp"

// Biblioteca de deteccao da placa e leitura do digito.
#include "../placa.hpp"

// ---------------------------------------------------------------------------
// Constantes de configuracao
// ---------------------------------------------------------------------------

// Tamanho do quadro recebido do Raspberry.
constexpr int NL = 240;
constexpr int NC = 320;

// Fator de ampliacao SOMENTE da janela de visualizacao (imshow). O video
// gravado continua em 440x240; a janela aparece com 880x480 para ficar
// legivel na tela do computador.
constexpr int ESCALA_TELA = 2;

// Controle proporcional para seguir a placa (igual ao cliente4):
//   BASE  = velocidade media para frente;
//   GANHO = quanto aumentar/diminuir a velocidade de cada roda quando a placa
//           esta fora do centro.
// GANHO maior corrige o desvio mais rapido (foi de 40 para 55).
constexpr int BASE = 40, GANHO = 55;

// Zona morta do controle: se a placa esta a menos de 3% do centro, as duas
// rodas recebem BASE e o carrinho segue reto (evita ficar "serpenteando").
constexpr float ZONA_MORTA = 0.03f;

// Velocidades usadas nos movimentos comandados pelos digitos.
constexpr int V_GIRO   = 70;   // velocidade das rodas ao girar no lugar
constexpr int V_FRENTE = 50;   // velocidade das rodas ao ir para frente

// Tempos (em segundos) dos movimentos. Estes valores precisam ser CALIBRADOS
// no carrinho real: dependem da bateria, do atrito, do peso etc.
constexpr double T_PARADA   = 0.8;  // espera para o carrinho parar (inercia)
constexpr double T_GIRO_90  = 0.80; // tempo para girar 90 graus (era 0.45)
constexpr double T_GIRO_180 = 3.20; // tempo para girar 180 graus (era 0.90)
constexpr double T_FRENTE   = 1.5;  // tempo para passar por baixo da placa

// Quantas leituras do digito sao feitas antes de decidir. Vale o digito mais
// votado; isso reduz o efeito de uma leitura errada isolada.
constexpr int VOTOS = 3;

// ---------------------------------------------------------------------------
// Estados da maquina de estados finita (numeracao da figura 6 do enunciado).
//
//   PROCURA   (0): procura a placa e segue em direcao a ela;
//   PARA      (1): para os motores e espera o carrinho parar;
//   LE_DIGITO (2): le o digito com o carrinho parado;
//   MOVE      (3): executa o movimento indicado pelo digito;
//   FIM       (4): digito 0 ou 1 -> termina o programa;
//   ESPERA    (5): espera o movimento cessar e volta para PROCURA.
// ---------------------------------------------------------------------------
enum Estado { PROCURA = 0, PARA = 1, LE_DIGITO = 2, MOVE = 3, FIM = 4, ESPERA = 5 };

// Um movimento e' composto por:
//   esq, dir = velocidades das rodas esquerda/direita (-100..100);
//   duracao  = quanto tempo o comando fica ativo (segundos).
struct Movimento {
  int esq;
  int dir;
  double duracao;
};

// ---------------------------------------------------------------------------
// Tabela digito -> movimento (comandos de 2025 do enunciado):
//
//   0 ou 1: pare o carrinho (fim do programa);
//   2:      vire 180 graus a esquerda imediatamente;
//   3:      vire 180 graus a direita imediatamente;
//   4 ou 5: passe por baixo da placa e continue em frente;
//   6 ou 7: vire 90 graus a esquerda imediatamente;
//   8 ou 9: vire 90 graus a direita imediatamente.
//
// Para girar no lugar, as rodas giram em sentidos opostos:
//   - esquerda = -V e direita = +V  -> gira para a esquerda;
//   - esquerda = +V e direita = -V  -> gira para a direita.
// ---------------------------------------------------------------------------
Movimento movimento_do_digito(int digito) {
  switch (digito) {
    case 2:  return { -V_GIRO, +V_GIRO, T_GIRO_180 };          // 180 graus a esquerda
    case 3:  return { +V_GIRO, -V_GIRO, T_GIRO_180 };          // 180 graus a direita
    case 4: case 5: return { V_FRENTE, V_FRENTE, T_FRENTE };   // passa por baixo e segue
    case 6: case 7: return { -V_GIRO, +V_GIRO, T_GIRO_90 };    // 90 graus a esquerda
    case 8: case 9: return { +V_GIRO, -V_GIRO, T_GIRO_90 };    // 90 graus a direita
  }

  // 0, 1 ou qualquer valor inesperado: fica parado.
  return { 0, 0, 0 };
}

// ---------------------------------------------------------------------------
// Controle proporcional para seguir a placa.
//
// Recebe cx = coordenada x do centro da placa no quadro.
// Calcula:
//   e    = erro normalizado (-1 = placa na borda esquerda, +1 = borda direita);
//   dirx = erro em porcentagem (so para mostrar na tela);
//   esq  = velocidade da roda esquerda;
//   dir  = velocidade da roda direita.
//
// Se a placa esta a direita (e>0), a roda esquerda anda mais rapido e a direita
// mais devagar, fazendo o carrinho virar para a direita.
// ---------------------------------------------------------------------------
void segue_placa(float cx, int& esq, int& dir, int& dirx) {
  float e = (cx - NC / 2.0f) / (NC / 2.0f);  // -1 (esquerda) .. +1 (direita)
  dirx = cvRound(100 * e);

  // Perto do centro: segue reto com as duas rodas iguais. Isso evita que o
  // controle fique corrigindo ruido de 1-2 pixels e "serpenteie".
  if (std::fabs(e) < ZONA_MORTA) {
    esq = dir = BASE;
    return;
  }

  // Limita as velocidades em -100..100 para nao passar do que o PWM aceita.
  esq = std::max(-100, std::min(100, cvRound(BASE + GANHO * e)));
  dir = std::max(-100, std::min(100, cvRound(BASE - GANHO * e)));
}

// ---------------------------------------------------------------------------
// Programa principal
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
  // -------------------------------------------------------------------------
  // 1) Linha de comando.
  // -------------------------------------------------------------------------
  if (argc < 2)
    erro("Uso: cliente6 IP [videosaida.avi] [t/c]\n"
         "Exemplo: ./client/cliente6 192.168.15.3 saida.avi t");

  bool gravar = (argc >= 3);                  // tem video de saida?
  char modo = (argc >= 4) ? argv[3][0] : 't'; // 't' = tela, 'c' = camera crua

  // Limiar do NCC usado para considerar que a placa foi encontrada.
  // O padrao e' 0.55, igual ao fase3p da aula 4. Para testes com iluminacao
  // ruim, pode-se baixar sem recompilar, por exemplo:
  //   NCC_MIN=0.45 ./client/cliente6 192.168.160.47
  float limiar_ncc = LIMIAR_NCC;
  if (const char* s = getenv("NCC_MIN")) limiar_ncc = (float)atof(s);

  // -------------------------------------------------------------------------
  // 2) Carrega os modelos da placa e o classificador de digitos.
  //
  // Os caminhos sao relativos: execute o programa a partir da pasta aula_05.
  // -------------------------------------------------------------------------
  Modelos mod = carrega_modelos("assets/quadrado.png");
  ClassificadorDigitos clf(PASTA_MNIST);

  // -------------------------------------------------------------------------
  // 3) Conecta ao servidor6 no Raspberry.
  // -------------------------------------------------------------------------
  CLIENT client(argv[1]);

  // -------------------------------------------------------------------------
  // 4) Variaveis de tela/gravacao.
  //
  // "mostrar" permite rodar sem interface grafica (por exemplo, em um teste
  // automatico). Se DISPLAY nao existir, nao chamamos imshow/namedWindow.
  // -------------------------------------------------------------------------
  Mat_<COR> a, d, tela;
  Mat_<float> f;
  VideoWriter vo;
  bool mostrar = (getenv("DISPLAY") != nullptr);

  // Modo de diagnostico (nao altera a logica normal):
  //   SEM_MOTOR=1 -> envia sempre velocidade 0 (o carrinho NAO anda);
  //   VERBOSE=1   -> imprime estado, escala, NCC e digito de cada quadro.
  // Uso: SEM_MOTOR=1 VERBOSE=1 ./client/cliente6 192.168.160.47
  const bool sem_motor = (getenv("SEM_MOTOR") != nullptr);
  const bool verboso   = (getenv("VERBOSE")   != nullptr);
  if (mostrar) namedWindow("janela");

  // Painel lateral inicial vazio (largura 120, altura NL).
  Mat_<COR> painel = monta_painel(Mat_<COR>(), Mat_<uchar>(), -1, NL);

  // -------------------------------------------------------------------------
  // 5) Estado inicial da maquina de estados.
  // -------------------------------------------------------------------------
  Estado estado = PROCURA;
  double t0 = 0;                    // cronometro do estado atual
  int votos[10] = {0}, nvotos = 0;  // contagem de leituras do digito
  int digito = -1;                  // ultimo digito decidido
  Movimento mov = {0, 0, 0};        // movimento atual

  // -------------------------------------------------------------------------
  // 6) Protocolo com o servidor6:
  //
  //    cmd[0] = '0' para executar; 's' para encerrar;
  //    cmd[1] = velocidade da roda esquerda + 100 (0..200);
  //    cmd[2] = velocidade da roda direita  + 100 (0..200).
  //
  // O servidor responde a cada comando com um novo quadro JPEG. O primeiro
  // comando e' enviado parado (100-100=0) so para pedir o primeiro quadro.
  // -------------------------------------------------------------------------
  BYTE cmd[3] = { '0', 100, 100 };
  client.sendBytes(3, cmd);  // pede o primeiro quadro

  // -------------------------------------------------------------------------
  // 7) Laco principal: recebe um quadro, decide o comando e envia.
  // -------------------------------------------------------------------------
  while (true) {
    // Recebe o quadro mais recente do Raspberry (JPEG comprimido).
    client.receiveImgComp(a);
    if (a.empty()) erro("quadro vazio");

    // Tempo atual, usado pelos cronometros dos estados.
    double agora = timeSinceEpoch();

    // Por padrao, o comando e' "parado".
    int esq = 0, dir = 0, dirx = 0;
    bool detectou = false;
    Deteccao det;
    LeituraDigito le;

    // ---------------------------------------------------------------------
    // ESTADO 0 (PROCURA): procura a placa e segue em direcao a ela.
    //
    // - Se nao achou placa: continua parado (esq=dir=0).
    // - Se achou placa pequena (escala > 1): segue a placa pelo centro.
    // - Se achou placa grande (escala 0 ou 1): para e vai para o estado 1.
    // ---------------------------------------------------------------------
    if (estado == PROCURA) {
      converte(a, f);
      det = detecta_placa(f, mod, limiar_ncc);
      detectou = true;

      if (det.achou) {
        int k = det.melhor;

        if (det.escala[k] <= ESCALA_PERTO) {
          // Placa grande: o carrinho esta perto. Para para nao colidir e para
          // poder ler o digito sem borrao de movimento.
          estado = PARA;
          t0 = agora;
        } else {
          // Placa pequena: segue em direcao a ela.
          float cx = det.pos[k].x + mod.m[det.escala[k]].cols / 2.0f;
          segue_placa(cx, esq, dir, dirx);
        }
      }
      // Se nao achou placa, esq=dir=0 (parado).
    }

    // ---------------------------------------------------------------------
    // ESTADO 1 (PARA): espera o carrinho parar completamente (inercia).
    // ---------------------------------------------------------------------
    else if (estado == PARA) {
      if (agora - t0 >= T_PARADA) {
        estado = LE_DIGITO;
        nvotos = 0;
        std::fill(votos, votos + 10, 0);
      }
    }

    // ---------------------------------------------------------------------
    // ESTADO 2 (LE_DIGITO): le o digito com o carrinho parado.
    //
    // - Se nao conseguir ler (placa sumiu, digito ilegivel etc.): volta ao
    //   estado 0.
    // - Se ler: acumula o voto. Depois de VOTOS leituras, decide pelo digito
    //   mais votado.
    //     * digito 0 ou 1 -> estado 4 (FIM);
    //     * outros        -> monta o movimento e vai para o estado 3.
    // ---------------------------------------------------------------------
    else if (estado == LE_DIGITO) {
      converte(a, f);
      det = detecta_placa(f, mod, limiar_ncc);
      detectou = true;

      le = le_digito(a, mod, det, clf);

      if (le.digito < 0) {
        // Nao conseguiu ler: volta a procurar a placa.
        estado = PROCURA;
      } else {
        // Atualiza o painel de depuracao com a leitura atual.
        painel = monta_painel(le.recorte, le.d14, le.digito, NL);

        votos[le.digito]++;
        nvotos++;

        if (nvotos >= VOTOS) {
          // Escolhe o digito com mais votos (max_element devolve o iterador).
          digito = (int)(std::max_element(votos, votos + 10) - votos);

          if (digito <= 1) {
            // 0 ou 1: pare e termine o programa.
            estado = FIM;
          } else {
            // 2..9: executa o movimento correspondente.
            mov = movimento_do_digito(digito);
            estado = MOVE;
            t0 = agora;
          }
        }
      }
    }

    // ---------------------------------------------------------------------
    // ESTADO 3 (MOVE): executa o movimento indicado pelo digito.
    //
    // O comando e' reenviado a cada quadro (o servidor so executa o ultimo
    // comando recebido). Quando o tempo programado termina, manda parar e vai
    // para o estado 5.
    // ---------------------------------------------------------------------
    else if (estado == MOVE) {
      esq = mov.esq;
      dir = mov.dir;

      if (agora - t0 >= mov.duracao) {
        estado = ESPERA;
        t0 = agora;
        esq = dir = 0;
      }
    }

    // ---------------------------------------------------------------------
    // ESTADO 5 (ESPERA): espera o movimento cessar completamente (inercia)
    // e volta para o estado 0.
    // ---------------------------------------------------------------------
    else if (estado == ESPERA) {
      if (agora - t0 >= T_PARADA)
        estado = PROCURA;
    }

    // O ESTADO 4 (FIM) e' tratado no final do laco, depois de montar a tela.

    // ---------------------------------------------------------------------
    // 7b) Diagnostico no terminal (apenas com VERBOSE=1).
    //
    // Mostra o que a deteccao viu neste quadro. Serve para descobrir se o
    // problema e' "nao achar a placa" (NCC baixo) ou "achar mas nao ler".
    // ---------------------------------------------------------------------
    if (verboso) {
      if (detectou && det.melhor >= 0) {
        int k = det.melhor;
        printf("estado=%d achou=%d escala=%d cc=%.2f ncc=%.2f",
               (int)estado, (int)det.achou, det.escala[k],
               det.cc[k], det.ncc[k]);
        if (le.digito >= 0) printf(" digito=%d", le.digito);
        printf("\n");
      } else {
        printf("estado=%d sem-placa\n", (int)estado);
      }
      fflush(stdout);
    }

    // ---------------------------------------------------------------------
    // 8) Tela de depuracao.
    // ---------------------------------------------------------------------
    a.copyTo(d);

    // Desenha a deteccao somente nos estados em que ela foi calculada.
    if (detectou) desenha_deteccao(d, mod, det);

    // HUD com estado, erro de direcao, ultimo digito e numero de votos.
    char hud[96];
    snprintf(hud, sizeof hud, "estado=%d dirx=%d digito=%d conta=%d",
             (int)estado, dirx, digito, nvotos);
    putText(d, hud, Point(5, NL - 8), FONT_HERSHEY_SIMPLEX, 0.4,
            Scalar(0, 0, 255), 1);

    // Junta o quadro com o painel lateral (resultado 440x240).
    hconcat(d, painel, tela);

    // Mostra a janela (somente se houver interface grafica).
    // A imagem e' ampliada ESCALA_TELA vezes SO para visualizacao; o video
    // gravado abaixo continua com 440x240.
    if (mostrar) {
      Mat_<COR> tela_grande;
      resize(tela, tela_grande, Size(), ESCALA_TELA, ESCALA_TELA, INTER_NEAREST);
      imshow("janela", tela_grande);
    }

    // ---------------------------------------------------------------------
    // 9) Gravacao do video de saida (opcional).
    //
    //   modo 'c' = grava somente a imagem crua da camera;
    //   modo 't' = grava a tela de depuracao (quadro + painel + HUD).
    // ---------------------------------------------------------------------
    if (gravar) {
      Mat saida = (modo == 'c') ? Mat(a) : Mat(tela);
      if (!vo.isOpened())
        vo.open(argv[2], VideoWriter::fourcc('M','J','P','G'), 15, saida.size());
      vo << saida;
    }

    // ---------------------------------------------------------------------
    // 10) Verifica se deve terminar.
    //
    // ESC ou estado 4 (digito 0/1) -> envia 's' para o servidor parar os
    // motores e encerra o laco.
    // ---------------------------------------------------------------------
    int ch = -1;
    if (mostrar) ch = waitKey(1) & 0xff;

    if (ch == 27 || estado == FIM) {
      cmd[0] = 's';
      cmd[1] = 100;  // velocidade esquerda 0
      cmd[2] = 100;  // velocidade direita 0
      client.sendBytes(3, cmd);
      break;
    }

    // ---------------------------------------------------------------------
    // 11) Envia o comando deste quadro.
    //
    // O servidor executa as velocidades e devolve o proximo quadro. Por isso
    // o laco e' sincrono: um comando -> um quadro.
    // ---------------------------------------------------------------------
    // No modo de diagnostico, zera as velocidades para o carrinho ficar parado.
    if (sem_motor) esq = dir = 0;

    cmd[0] = '0';
    cmd[1] = esq + 100;
    cmd[2] = dir + 100;
    client.sendBytes(3, cmd);
  }

  return 0;
}
