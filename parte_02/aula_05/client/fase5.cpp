//fase5.cpp -- Licao de casa 1 da fase 5 (vale 5).
//
// O QUE O PROGRAMA FAZ:
//   1) le o quadrado.png e monta os 10 modelos da placa;
//   2) le o MNIST e treina o FlaNN (vizinho mais proximo com FLANN);
//   3) para cada quadro do video de entrada:
//        - localiza a placa (placa.hpp);
//        - desenha os candidatos e o melhor candidato;
//        - se a placa estiver perto (escala 0 ou 1), le o digito;
//        - monta um painel lateral com o digito 14x14 e o recorte colorido;
//        - grava o quadro (com o painel) no video de saida;
//   4) no final, imprime um resumo do processamento.
//
// COMPILAR (no computador, dentro da pasta aula_05):
//   g++ -O3 -fopenmp -std=c++11 client/fase5.cpp -o client/fase5 \
//       $(pkg-config --cflags --libs opencv4)
//
// EXECUTAR (dentro da pasta aula_05, para achar assets/mnist):
//   ./client/fase5 capturado.avi assets/quadrado.png locarec.avi
//
// OBSERVACOES:
//   - O programa NAO abre janela; ele apenas grava o video. Isso permite rodar
//     em qualquer terminal, inclusive sem interface grafica.
//   - O video de saida tem 440x240: 320x240 do video + 120 de largura do painel.
#include <opencv2/opencv.hpp>
#include <omp.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

// Inclui a versao COMPLETA do raspberry.hpp (com MNIST, MnistFlann, converte,
// somaAbsDois, dcReject etc.). Ela esta na copia lib/raspberry.hpp; os arquivos
// originais da aula_05 e da aula_04 nao foram alterados.
#include "../lib/raspberry.hpp"

// Inclui a biblioteca de deteccao/leitura da placa. O placa.hpp depende das
// funcoes do raspberry.hpp incluido acima.
#include "../placa.hpp"

int main(int argc, char** argv) {
  // -------------------------------------------------------------------------
  // 1) Verifica a linha de comando.
  //
  // Uso: fase5 <video_entrada.avi> <quadrado.png> <video_saida.avi>
  // Exemplo: fase5 capturado.avi assets/quadrado.png locarec.avi
  // -------------------------------------------------------------------------
  if (argc != 4) {
    erro("Uso: fase5 video_entrada.avi quadrado.png video_saida.avi\n"
         "Exemplo: ./client/fase5 capturado.avi assets/quadrado.png locarec.avi");
  }

  const std::string arq_entrada = argv[1];  // video com a camera do carrinho
  const std::string arq_placa   = argv[2];  // imagem da placa (quadrado.png)
  const std::string arq_saida   = argv[3];  // video que sera gerado

  // -------------------------------------------------------------------------
  // 2) Carrega os modelos da placa.
  //
  // carrega_modelos lê o quadrado.png, acha a janela do digito e cria 10
  // modelos em escalas diferentes. Isso e' feito uma unica vez.
  // -------------------------------------------------------------------------
  std::cout << "Carregando modelos da placa..." << std::endl;
  Modelos mod = carrega_modelos(arq_placa);

  // -------------------------------------------------------------------------
  // 3) Le o MNIST e treina o FlaNN.
  //
  // O construtor de ClassificadorDigitos:
  //   - le os 60000 exemplos de treino de assets/mnist;
  //   - redimensiona para 14x14;
  //   - inverte as cores (fundo branco / tinta preta);
  //   - ajusta o bounding box;
  //   - constroi o indice FLANN.
  // -------------------------------------------------------------------------
  std::cout << "Lendo MNIST e treinando FlaNN (pode demorar alguns segundos)..."
            << std::endl;
  ClassificadorDigitos clf(PASTA_MNIST);
  std::cout << "FlaNN treinado." << std::endl;

  // -------------------------------------------------------------------------
  // 4) Abre o video de entrada.
  // -------------------------------------------------------------------------
  cv::VideoCapture cap(arq_entrada);
  if (!cap.isOpened()) erro("Nao consegui abrir o video de entrada: " + arq_entrada);

  // O enunciado usa quadros de 320x240. Se o video vier em outro tamanho,
  // redimensionamos para 320x240 para a deteccao funcionar como no fase3p.
  const int NL = 240;  // numero de linhas do quadro
  const int NC = 320;  // numero de colunas do quadro

  // -------------------------------------------------------------------------
  // 5) Cria o video de saida.
  //
  // A largura e' NC + 120 porque o painel lateral tem 120 pixels.
  // O codec MJPG gera um .avi que o VLC/OpenCV conseguem abrir.
  // A taxa de 30 fps e' a mesma do capturado.avi.
  // -------------------------------------------------------------------------
  const int LARG_PAINEL = 120;
  cv::VideoWriter out(arq_saida,
                      cv::VideoWriter::fourcc('M','J','P','G'),
                      30,
                      cv::Size(NC + LARG_PAINEL, NL));
  if (!out.isOpened()) erro("Nao consegui criar o video de saida: " + arq_saida);

  // -------------------------------------------------------------------------
  // 6) Variaveis de estatistica (para o resumo final).
  // -------------------------------------------------------------------------
  int n_quadros = 0;   // quantos quadros foram processados
  int n_achou   = 0;   // quantos quadros tiveram placa localizada
  int n_perto   = 0;   // quantos quadros tinham placa em escala 0 ou 1
  int n_lidos   = 0;   // quantos digitos foram lidos com sucesso
  std::vector<int> hist(10, 0);  // histograma dos digitos lidos (0..9)

  cv::Mat_<COR> a;     // quadro colorido de entrada
  cv::Mat_<float> f;   // quadro em cinza float (0=preto, 1=branco)

  // -------------------------------------------------------------------------
  // 7) Laco principal: processa um quadro por vez.
  // -------------------------------------------------------------------------
  while (cap.read(a)) {
    if (a.empty()) break;

    // Garante o tamanho 320x240 esperado pela deteccao.
    if (a.cols != NC || a.rows != NL)
      cv::resize(a, a, cv::Size(NC, NL));

    // Converte o quadro colorido para cinza float.
    converte(a, f);

    // Localiza a placa (CC -> candidatos -> NCC -> melhor candidato).
    Deteccao det = detecta_placa(f, mod);

    // Copia o quadro original para desenhar a depuracao em cima.
    cv::Mat_<COR> d;
    a.copyTo(d);

    // Desenha os candidatos, o melhor candidato e os textos de status.
    desenha_deteccao(d, mod, det);

    // Contabiliza a deteccao.
    if (det.achou) {
      n_achou++;

      // Escala 0 ou 1 = placa grande = camera perto.
      if (det.escala[det.melhor] <= ESCALA_PERTO) n_perto++;
    }

    // ---------------------------------------------------------------------
    // 8) Se a placa estiver perto, le o digito.
    //
    // le_digito faz:
    //   4a) recorta a janela do digito;
    //   4b) ajusta brilho/contraste;
    //   4c) bbox + 14x14;
    //   4d) classifica com FlaNN.
    // Se nao conseguir ler, devolve digito=-1 e as imagens ficam vazias.
    // ---------------------------------------------------------------------
    LeituraDigito le = le_digito(a, mod, det, clf);

    if (le.digito >= 0) {
      n_lidos++;
      hist[le.digito]++;

      // Escreve o digito lido no quadro principal (canto inferior esquerdo).
      cv::putText(d,
                  "Digito: " + std::to_string(le.digito),
                  cv::Point(5, NL - 10),
                  cv::FONT_HERSHEY_SIMPLEX,
                  0.7,
                  cv::Scalar(0, 0, 255),
                  2);
    }

    // ---------------------------------------------------------------------
    // 9) Monta o painel lateral e junta com o quadro principal.
    //
    // O painel mostra:
    //   - em cima: o digito 14x14 ampliado;
    //   - embaixo: o recorte colorido da janela do digito;
    //   - em vermelho: o digito lido.
    // ---------------------------------------------------------------------
    cv::Mat_<COR> painel = monta_painel(le.recorte, le.d14, le.digito, NL);

    // hconcat coloca o painel a direita do quadro: resultado 440x240.
    cv::Mat_<COR> saida;
    cv::hconcat(d, painel, saida);

    // Grava o quadro no video de saida.
    out << saida;

    n_quadros++;
  }

  // -------------------------------------------------------------------------
  // 10) Fecha os arquivos e imprime o resumo.
  // -------------------------------------------------------------------------
  cap.release();
  out.release();

  std::cout << "\n===== RESUMO =====\n";
  std::cout << "Quadros processados : " << n_quadros << "\n";
  std::cout << "Quadros com placa   : " << n_achou << "\n";
  std::cout << "Placa perto (0/1)   : " << n_perto << "\n";
  std::cout << "Digitos lidos       : " << n_lidos << "\n";
  std::cout << "Histograma (0..9)   : ";
  for (int i = 0; i < 10; i++) std::cout << hist[i] << (i == 9 ? "\n" : " ");
  std::cout << "Video gerado        : " << arq_saida << std::endl;

  return 0;
}
