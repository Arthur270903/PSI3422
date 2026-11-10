//placa.hpp -- deteccao da placa e leitura do digito (fase5 e cliente6).
//
// Fluxo:
//   carrega_modelos()  le quadrado.png e monta os modelos em varias escalas;
//   detecta_placa()    CC -> 20 picos -> NCC -> melhor candidato;
//   le_digito()        recorta o digito, ajusta o contraste, reduz para 14x14
//                      e classifica com o FlaNN (MNIST);
//   monta_painel()     monta o painel lateral de depuracao.
//
// Depende do raspberry.hpp completo (converte, dcReject, somaAbsDois, erro,
// COR, MnistFlann). Inclua "../lib/raspberry.hpp" (ou projeto.hpp) ANTES.
#pragma once
#include <opencv2/opencv.hpp>
#include <omp.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

// ===========================================================================
// Configuracao
// ===========================================================================

// Quantidade de modelos (tamanhos) gerados a partir de quadrado.png.
constexpr int    NUMBER_OF_SCALES = 10;

// fase5: escala 0 ou 1 = placa grande = camera perto (unico caso em que le).
constexpr int    ESCALA_PERTO     = 1;

// Limiar de aceitacao do NCC para considerar a placa localizada.
constexpr float  LIMIAR_NCC       = 0.55f;

// O digito e' normalizado para 14x14, igual ao MNIST do treino.
constexpr int    TAM_DIGITO       = 14;

// Margem descartada na borda da janela (evita a borda branca da placa).
constexpr int    MARGEM_JANELA    = 1;

// Contraste minimo (brilho fundo - brilho tinta) para aceitar que ha digito.
constexpr double CONTRASTE_MIN    = 30.0;

// Arquivos do MNIST (train-images.idx3-ubyte etc.).
const std::string PASTA_MNIST     = "assets/mnist";

// ===========================================================================
// Modelos
// ===========================================================================

struct Modelos {
  std::vector<cv::Mat_<float>> m;  // um modelo por escala (dcReject + somaAbsDois)
  cv::Rect janela;                 // janela do digito no quadrado.png original
  int lado_original = 0;           // lado do quadrado.png (401)
};

// Le a placa, acha a janela do digito e gera n_escalas modelos em progressao
// geometrica entre escala_max e escala_min.
//
// Os valores padrao reproduzem o fase3p/fase5. O cliente6 passa uma faixa
// ampliada (placas maiores) para poder se aproximar mais antes de ler.
Modelos carrega_modelos(const std::string& arquivo,
                        int n_escalas    = NUMBER_OF_SCALES,
                        float escala_max = 0.1721f,
                        float escala_min = 0.0473f) {
  if (n_escalas < 2) n_escalas = 2;

  Modelos mod;

  cv::Mat_<COR> temp = cv::imread(arquivo, cv::IMREAD_COLOR);
  if (temp.empty()) erro("Falha na leitura de " + arquivo);

  // Cinza em float, 0=preto e 1=branco.
  cv::Mat_<float> T;
  converte(temp, T);
  mod.lado_original = T.rows;

  // O miolo do quadrado e' branco puro (255,255,255); os demais brancos tem
  // 254 em algum canal. A mascara isola so o miolo. A componente conexa que
  // contem o centro da placa da' a janela do digito.
  cv::Mat mascara;
  cv::inRange(temp, cv::Scalar(255,255,255), cv::Scalar(255,255,255), mascara);
  cv::Mat rotulos, stats, centroides;
  cv::connectedComponentsWithStats(mascara, rotulos, stats, centroides, 4);

  int rotulo = rotulos.at<int>(temp.rows/2, temp.cols/2);
  if (rotulo == 0)
    erro("Centro da placa nao e branco puro: janela do digito nao encontrada");

  mod.janela = cv::Rect(stats.at<int>(rotulo, cv::CC_STAT_LEFT),
                        stats.at<int>(rotulo, cv::CC_STAT_TOP),
                        stats.at<int>(rotulo, cv::CC_STAT_WIDTH),
                        stats.at<int>(rotulo, cv::CC_STAT_HEIGHT));

  // INTER_NEAREST e' proposital: nao cria cores novas e preserva o valor 1.0
  // ("don't care") usado pelo dcReject.
  mod.m.resize(n_escalas);
  for (int i = 0; i < n_escalas; i++) {
    float escala = escala_max * std::pow(escala_min / escala_max,
                                        i / (float)(n_escalas - 1));
    int lado = (int)std::round(T.rows * escala);

    cv::Mat_<float> reduzido;
    cv::resize(T, reduzido, cv::Size(lado, lado), 0, 0, cv::INTER_NEAREST);

    // dcReject: remove o nivel DC e trata 1.0 como "don't care".
    // somaAbsDois: normaliza a soma dos absolutos, deixando o CC comparavel
    // entre escalas diferentes.
    mod.m[i] = somaAbsDois(dcReject(reduzido, 1.0f));
  }
  return mod;
}

// ===========================================================================
// Deteccao
// ===========================================================================

struct Deteccao {
  bool achou = false;                    // melhor NCC passou do limiar?
  int  melhor = -1;                      // indice do melhor candidato
  std::vector<int>       escala;         // dados de cada candidato
  std::vector<cv::Point> pos;
  std::vector<float>     cc, ncc;
};

// Mesma logica do fase3p (aula 4).
Deteccao detecta_placa(const cv::Mat_<float>& f, const Modelos& mod) {
  const std::vector<cv::Mat_<float>>& models = mod.m;
  const int n_escalas = (int)models.size();
  Deteccao det;

  // (1) CC de todas as escalas (paralelo: cada thread escreve em cc[i]).
  std::vector<cv::Mat_<float>> cc(n_escalas);
  #pragma omp parallel for
  for (int i = 0; i < n_escalas; i++)
    cv::matchTemplate(f, models[i], cc[i], cv::TM_CCORR);

  // (2) 20 maiores picos de CC, separados por pelo menos 10 pixels.
  for (int k = 0; k < 20; k++) {
    float maior = -1.0e30f;
    int melhor_escala = -1;
    cv::Point melhor_pos;

    for (int i = 0; i < n_escalas; i++) {
      double maximo; cv::Point p;
      cv::minMaxLoc(cc[i], nullptr, &maximo, nullptr, &p);
      if (maximo > maior) { maior = (float)maximo; melhor_escala = i; melhor_pos = p; }
    }

    if (melhor_escala == -1 || maior <= -1.0e29) break;

    det.escala.push_back(melhor_escala);
    det.pos.push_back(melhor_pos);
    det.cc.push_back(maior);

    // Compara CENTROS (o canto muda de escala para escala) e apaga os
    // vizinhos em todas as escalas para nao repetir o mesmo pico.
    int cx = melhor_pos.x + models[melhor_escala].cols / 2;
    int cy = melhor_pos.y + models[melhor_escala].rows / 2;
    for (int i = 0; i < n_escalas; i++) {
      int xt = cx - models[i].cols / 2;
      int yt = cy - models[i].rows / 2;
      for (int y = std::max(0, yt - 9); y <= std::min(cc[i].rows - 1, yt + 9); y++)
        for (int x = std::max(0, xt - 9); x <= std::min(cc[i].cols - 1, xt + 9); x++)
          if ((x-xt)*(x-xt) + (y-yt)*(y-yt) < 100)
            cc[i](y, x) = -1.0e30f;
    }
  }

  // (3) NCC de todas as escalas: invariante a brilho/contraste, usado para
  // confirmar o candidato.
  std::vector<cv::Mat_<float>> ncc(n_escalas);
  #pragma omp parallel for
  for (int i = 0; i < n_escalas; i++)
    cv::matchTemplate(f, models[i], ncc[i], cv::TM_CCOEFF_NORMED);

  // (4) Consulta o NCC somente nos candidatos.
  float maior_ncc = -1.0f;
  for (int k = 0; k < (int)det.escala.size(); k++) {
    float v = ncc[det.escala[k]](det.pos[k].y, det.pos[k].x);
    det.ncc.push_back(v);
    if (v > maior_ncc) maior_ncc = v;
    det.melhor = k;   // mantem o ultimo candidato (igual ao fase3p)
  }

  det.achou = (det.melhor >= 0 && maior_ncc > LIMIAR_NCC);
  return det;
}

// Candidatos em azul, melhor candidato em amarelo, textos de escala/CC/NCC.
void desenha_deteccao(cv::Mat_<COR>& d, const Modelos& mod, const Deteccao& det) {
  for (int k = 0; k < (int)det.escala.size(); k++)
    cv::rectangle(d, cv::Rect(det.pos[k].x, det.pos[k].y,
                              mod.m[det.escala[k]].cols, mod.m[det.escala[k]].rows),
                  cv::Scalar(255,0,0), 1);

  if (det.melhor >= 0) {
    int k = det.melhor;

    if (det.achou)
      cv::rectangle(d, cv::Rect(det.pos[k].x, det.pos[k].y,
                                mod.m[det.escala[k]].cols, mod.m[det.escala[k]].rows),
                    cv::Scalar(0,255,255), 2);

    std::string texto = "Escala: " + std::to_string(det.escala[k]) +
                        " CC: "  + std::to_string((int)std::round(det.cc[k]  * 100)) + "%" +
                        " NCC: " + std::to_string((int)std::round(det.ncc[k] * 100)) + "%";

    cv::rectangle(d, cv::Rect(0, 0, d.cols, 25), cv::Scalar(0,0,0), cv::FILLED);
    cv::putText(d, texto, cv::Point(5,18), cv::FONT_HERSHEY_SIMPLEX, 0.45,
                cv::Scalar(0,255,255), 1);
  }

  cv::putText(d, det.achou ? "Localizada" : "Nao Localizada", cv::Point(5,43),
              cv::FONT_HERSHEY_SIMPLEX, 0.45,
              det.achou ? cv::Scalar(0,255,0) : cv::Scalar(0,0,255), 1);
}

// ===========================================================================
// Preparo do digito
// ===========================================================================

// Retangulo do digito no quadro atual, por regra de tres a partir da janela
// medida no quadrado.png (401x401). Corta na borda do quadro (& de Rect).
cv::Rect janela_digito(const Modelos& mod, int escala, cv::Point pos,
                       cv::Size tam_quadro) {
  double k = (double)mod.m[escala].rows / mod.lado_original;
  int x = pos.x + cvRound(mod.janela.x * k) + MARGEM_JANELA;
  int y = pos.y + cvRound(mod.janela.y * k) + MARGEM_JANELA;
  int w = cvRound(mod.janela.width  * k) - 2 * MARGEM_JANELA;
  int h = cvRound(mod.janela.height * k) - 2 * MARGEM_JANELA;

  if (w < 4 || h < 4) return cv::Rect();

  return cv::Rect(x, y, w, h) & cv::Rect(0, 0, tam_quadro.width, tam_quadro.height);
}

// Passo 4b: fundo -> branco (255) e tinta -> preto (0). Devolve vazio se a
// janela nao tem contraste suficiente (sem digito).
cv::Mat_<uchar> ajusta_contraste(const cv::Mat_<uchar>& g) {
  // Otsu separa automaticamente fundo (255) e tinta (0).
  cv::Mat bin;
  cv::threshold(g, bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

  int n_fundo = cv::countNonZero(bin);
  int n_tinta = (int)bin.total() - n_fundo;
  if (n_fundo == 0 || n_tinta == 0) return cv::Mat_<uchar>();

  // Mascara da tinta e brilho medio de cada grupo.
  cv::Mat inv; cv::bitwise_not(bin, inv);
  double fundo = cv::mean(g, bin)[0];
  double tinta = cv::mean(g, inv)[0];

  if (fundo - tinta < CONTRASTE_MIN) return cv::Mat_<uchar>();

  // tinta -> 0 e 75% do caminho ate o fundo -> 255. O fundo satura em branco
  // puro, que e' o que o bbox do MNIST espera.
  double alto = tinta + 0.75 * (fundo - tinta);
  cv::Mat_<uchar> saida;
  g.convertTo(saida, CV_8U, 255.0 / (alto - tinta), -tinta * 255.0 / (alto - tinta));
  return saida;
}

// ===========================================================================
// Classificador
// ===========================================================================

// FlaNN (vizinho mais proximo com FLANN) sobre os 60000 exemplos do MNIST.
class ClassificadorDigitos {
  MnistFlann mnist;

public:
  // MnistFlann(14, true, true): reduz para 14x14, inverte as cores e ajusta
  // o bounding box. le(pasta, 60000, 0): so treino, sem os 10000 de teste.
  ClassificadorDigitos(const std::string& pasta) : mnist(TAM_DIGITO, true, true) {
    mnist.le(pasta, 60000, 0);
    mnist.train();
  }

  // Passo 4c: bbox da tinta + reducao para 14x14 (vazio se nao ha tinta).
  cv::Mat_<float> normaliza_digito(const cv::Mat_<float>& a) {
    cv::Mat_<float> b = mnist.bbox(a);
    if (!mnist.localizou) return cv::Mat_<float>();
    return b;
  }

  // Passo 4d: digito 14x14 -> rotulo 0..9.
  int classifica(const cv::Mat_<float>& b) {
    // O FLANN espera uma linha de 196 valores por consulta.
    cv::Mat_<float> q(1, b.rows * b.cols);
    for (int l = 0; l < b.rows; l++)
      for (int c = 0; c < b.cols; c++)
        q(0, l * b.cols + c) = b(l, c);

    return cvRound(mnist.predictInterno(q));
  }
};

// ===========================================================================
// Leitura de um quadro
// ===========================================================================

struct LeituraDigito {
  int digito = -1;             // -1 = nao leu; 0..9 = leu
  cv::Mat_<COR>   recorte;     // passo 4a (janela colorida)
  cv::Mat_<uchar> d14;         // passo 4c (digito 14x14)
};

// lado_minimo < 0 -> criterio do fase5 (escala <= ESCALA_PERTO);
// lado_minimo > 0 -> exige esse lado minimo em pixels (criterio do cliente6).
LeituraDigito le_digito(const cv::Mat_<COR>& a, const Modelos& mod,
                        const Deteccao& det, ClassificadorDigitos& clf,
                        int lado_minimo = -1) {
  LeituraDigito r;

  if (!det.achou) return r;
  int k = det.melhor;
  if (lado_minimo > 0) {
    if (mod.m[det.escala[k]].cols < lado_minimo) return r;
  } else {
    if (det.escala[k] > ESCALA_PERTO) return r;
  }

  cv::Rect j = janela_digito(mod, det.escala[k], det.pos[k], a.size());
  if (j.empty()) return r;

  // 4a) recorte colorido.
  r.recorte = a(j).clone();

  // 4b) cinza + brilho/contraste.
  cv::Mat_<uchar> cinza;
  cv::cvtColor(r.recorte, cinza, cv::COLOR_BGR2GRAY);
  cv::Mat_<uchar> ajustada = ajusta_contraste(cinza);
  if (ajustada.empty()) return r;

  cv::Mat_<float> af;
  ajustada.convertTo(af, CV_32F, 1.0 / 255.0);

  // 4c) bbox + 14x14.
  cv::Mat_<float> b = clf.normaliza_digito(af);
  if (b.empty()) return r;
  b.convertTo(r.d14, CV_8U, 255.0);

  // 4d) FlaNN.
  r.digito = clf.classifica(b);
  return r;
}

// Painel lateral: digito 14x14 ampliado em cima, recorte colorido embaixo e o
// digito lido em vermelho. Largura 120 -> video final 440x240 (igual ao
// locarec.avi). INTER_NEAREST para mostrar os pixels reais.
cv::Mat_<COR> monta_painel(const cv::Mat_<COR>& recorte, const cv::Mat_<uchar>& d14,
                           int digito, int altura) {
  const int L = 120;
  cv::Mat_<COR> painel(altura, L, COR(128,128,128));

  if (!d14.empty()) {
    cv::Mat_<uchar> g;
    cv::resize(d14, g, cv::Size(L, L), 0, 0, cv::INTER_NEAREST);
    cv::Mat_<COR> c;
    cv::cvtColor(g, c, cv::COLOR_GRAY2BGR);
    c.copyTo(painel(cv::Rect(0, 0, L, L)));
  }

  if (!recorte.empty()) {
    cv::Mat_<COR> r;
    cv::resize(recorte, r, cv::Size(L, L), 0, 0, cv::INTER_NEAREST);
    r.copyTo(painel(cv::Rect(0, L, L, L)));
  }

  if (digito >= 0)
    cv::putText(painel, std::to_string(digito), cv::Point(25, L + 60),
                cv::FONT_HERSHEY_SIMPLEX, 2.0, cv::Scalar(0,0,255), 3);

  return painel;
}
