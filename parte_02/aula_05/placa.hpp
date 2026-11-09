//placa.hpp -- biblioteca comum ao fase5.cpp (computador) e ao cliente6.cpp (computador).
//
// O QUE ESTE ARQUIVO FAZ:
//   1) carrega o quadrado.png e monta 10 modelos da placa em escalas diferentes;
//   2) localiza a placa no quadro usando correlacao (CC) para achar candidatos e
//      correlacao normalizada (NCC) para confirmar o melhor candidato;
//   3) recorta o digito de dentro da placa, ajusta brilho/contraste, elimina as
//      bordas brancas, reduz para 14x14 e classifica com o FlaNN (MNIST).
//
// ORDEM DOS INCLUDES (importante):
//   Este arquivo NAO inclui o raspberry.hpp. Ele usa tipos e funcoes que estao
//   no raspberry.hpp completo (converte, somaAbsDois, dcReject, erro, COR, FLT,
//   MnistFlann). Por isso, nos programas .cpp, inclua primeiro
//   "../lib/raspberry.hpp" (ou "../lib/projeto.hpp") e DEPOIS este placa.hpp.
//
// OBSERVACAO SOBRE O RASPBERRY.HPP:
//   A copia completa do raspberry.hpp (com MNIST e MnistFlann) foi colocada em
//   aula_05/lib/raspberry.hpp. Os arquivos originais projeto.hpp e raspberry.hpp
//   da aula_05 (e tudo da aula_04) foram mantidos intactos.
#pragma once
#include <opencv2/opencv.hpp>
#include <omp.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

// ===========================================================================
// Constantes de configuracao
// ===========================================================================

// Numero de modelos (tamanhos) da placa gerados a partir de quadrado.png.
constexpr int    NUMBER_OF_SCALES = 10;

// Escala 0 e 1 = placa grande = camera perto. Somente nestas escalas o digito
// e lido (o digito precisa estar grande e nitido para o classificador).
constexpr int    ESCALA_PERTO     = 1;

// Limiar de aceitacao do NCC. Se o melhor NCC for maior que este valor,
// consideramos que a placa foi localizada no quadro.
constexpr float  LIMIAR_NCC       = 0.55f;

// O digito e sempre normalizado para 14x14, igual ao MNIST usado no treino.
constexpr int    TAM_DIGITO       = 14;

// Margem (em pixels) descartada na borda da janela do digito. Serve para nao
// pegar a borda amarela/branca da placa junto com o digito.
constexpr int    MARGEM_JANELA    = 1;

// Diferenca minima de brilho entre fundo e tinta para aceitar que existe um
// digito na janela. Se a janela estiver "lisa" (sem digito), rejeitamos.
constexpr double CONTRASTE_MIN    = 30.0;

// Pasta onde estao os quatro arquivos do MNIST (train-images.idx3-ubyte etc.).
// Os arquivos ficam em aula_05/assets/mnist/.
const std::string PASTA_MNIST     = "assets/mnist";

// ===========================================================================
// Modelos da placa
// ===========================================================================

// Um modelo para cada escala + a posicao da janela do digito dentro do
// quadrado.png original. O quadrado.png tem 401x401 pixels.
struct Modelos {
  std::vector<cv::Mat_<float>> m;  // modelo de cada escala (ja com dcReject e somaAbsDois)
  cv::Rect janela;                 // janela do digito, em coordenadas do quadrado.png
  int lado_original = 0;           // lado do quadrado.png (401)
};

// Le quadrado.png, acha a janela do digito e gera os 10 modelos.
// Esta funcao e chamada uma unica vez, no inicio do programa.
Modelos carrega_modelos(const std::string& arquivo) {
  Modelos mod;

  // Le a imagem colorida da placa. Se o arquivo nao existir, termina o programa.
  cv::Mat_<COR> temp = cv::imread(arquivo, cv::IMREAD_COLOR);
  if (temp.empty()) erro("Falha na leitura de " + arquivo);

  // Converte para cinza em float, com 0=preto e 1=branco.
  cv::Mat_<float> T;
  converte(temp, T);
  mod.lado_original = T.rows;  // 401

  // -------------------------------------------------------------------------
  // Acha a janela do digito dentro da placa.
  //
  // O interior do quadrado amarelo e formado por pixels EXATAMENTE brancos
  // (255,255,255). Os outros brancos da placa tem 254 em algum canal; por isso
  // a mascara abaixo isola apenas o miolo branco. Depois pegamos a componente
  // conexa (4-vizinhos) que contem o centro da placa e usamos o retangulo que
  // a envolve como janela do digito.
  // -------------------------------------------------------------------------
  cv::Mat mascara;
  cv::inRange(temp, cv::Scalar(255,255,255), cv::Scalar(255,255,255), mascara);
  cv::Mat rotulos, stats, centroides;
  cv::connectedComponentsWithStats(mascara, rotulos, stats, centroides, 4);
  int rotulo = rotulos.at<int>(temp.rows/2, temp.cols/2);
  if (rotulo == 0) erro("Centro da placa nao e branco puro: janela do digito nao encontrada");
  mod.janela = cv::Rect(stats.at<int>(rotulo, cv::CC_STAT_LEFT),  stats.at<int>(rotulo, cv::CC_STAT_TOP),
                        stats.at<int>(rotulo, cv::CC_STAT_WIDTH), stats.at<int>(rotulo, cv::CC_STAT_HEIGHT));

  // -------------------------------------------------------------------------
  // Gera 10 modelos em progressao geometrica: a escala vai de 0.1721
  // (aproximadamente 69x69 pixels) ate 0.0473 (aproximadamente 19x19 pixels).
  //
  // Usamos INTER_NEAREST de proposito: a interpolacao NEAREST nao cria cores
  // novas. O valor 1.0 e' o "don't care" (nao importa) usado pelo dcReject;
  // se interpolar, esse valor se misturaria com os vizinhos e perderia o
  // significado.
  // -------------------------------------------------------------------------
  mod.m.resize(NUMBER_OF_SCALES);
  for (int i = 0; i < NUMBER_OF_SCALES; i++) {
    // Escala do modelo i (progressao geometrica entre os dois extremos).
    float escala = 0.1721f * std::pow(0.0473f / 0.1721f, i / (float)(NUMBER_OF_SCALES - 1));

    // Lado do modelo em pixels.
    int lado = (int)std::round(T.rows * escala);

    // Redimensiona o quadrado.png para o tamanho do modelo.
    cv::Mat_<float> reduzido;
    cv::resize(T, reduzido, cv::Size(lado, lado), 0, 0, cv::INTER_NEAREST);

    // Pre-processamento igual ao fase3p:
    //   dcReject: subtrai a media (elimina o nivel DC) e trata o valor 1.0
    //             como "don't care";
    //   somaAbsDois: normaliza a soma dos valores absolutos para 2.0, para
    //                que o resultado da correlacao (CC) fique comparavel
    //                entre escalas diferentes.
    mod.m[i] = somaAbsDois(dcReject(reduzido, 1.0f));
  }
  return mod;
}

// ===========================================================================
// Deteccao da placa
// ===========================================================================

// Resultado da deteccao de um quadro:
//   achou  = true se o melhor candidato passou do LIMIAR_NCC;
//   melhor = indice do melhor candidato (-1 = nenhum);
//   escala, pos, cc, ncc = dados de cada candidato (para desenhar/depurar).
struct Deteccao {
  bool achou = false;
  int  melhor = -1;
  std::vector<int>       escala;
  std::vector<cv::Point> pos;
  std::vector<float>     cc, ncc;
};

// Recebe o quadro em cinza float (f) e os modelos (mod).
// Mesma logica do fase3p (aula 4): CC -> 20 picos -> NCC -> melhor candidato.
//
// O parametro limiar permite baixar/aumentar a exigencia do NCC sem mudar o
// algoritmo. O valor padrao e' LIMIAR_NCC (0.55), o mesmo do fase3p da aula 4.
Deteccao detecta_placa(const cv::Mat_<float>& f, const Modelos& mod,
                       float limiar = LIMIAR_NCC) {
  const std::vector<cv::Mat_<float>>& models = mod.m;
  const int n_escalas = (int)models.size();
  Deteccao det;

  // -------------------------------------------------------------------------
  // (1) Calcula os mapas de correlacao cruzada (CC) para todas as escalas.
  //
  // TM_CCORR e' a correlacao simples. Como os modelos foram normalizados por
  // somaAbsDois, os valores das escalas ficam comparaveis.
  //
  // O "#pragma omp parallel for" distribui as escalas entre as threads.
  // Cada iteracao escreve somente em cc[i], portanto nao ha condicao de corrida.
  // -------------------------------------------------------------------------
  std::vector<cv::Mat_<float>> cc(n_escalas);
  #pragma omp parallel for
  for (int i = 0; i < n_escalas; i++)
    cv::matchTemplate(f, models[i], cc[i], cv::TM_CCORR);

  // -------------------------------------------------------------------------
  // (2) Encontra os 20 maiores picos de CC, separados por pelo menos 10 pixels.
  //
  // Para cada pico escolhido:
  //   - guardamos escala, posicao e valor de CC;
  //   - apagamos (colocamos -1e30) os pixels vizinhos, em TODAS as escalas,
  //     para que o proximo pico seja uma placa diferente e nao o mesmo pico
  //     deslocado alguns pixels.
  // -------------------------------------------------------------------------
  for (int k = 0; k < 20; k++) {
    float maior = -1.0e30f;
    int melhor_escala = -1;
    cv::Point melhor_pos;

    // Procura o maior valor de CC entre todas as escalas.
    for (int i = 0; i < n_escalas; i++) {
      double maximo; cv::Point p;
      cv::minMaxLoc(cc[i], nullptr, &maximo, nullptr, &p);
      if (maximo > maior) { maior = (float)maximo; melhor_escala = i; melhor_pos = p; }
    }

    // Se nao ha mais picos validos, termina a busca.
    if (melhor_escala == -1 || maior <= -1.0e29) break;

    det.escala.push_back(melhor_escala);
    det.pos.push_back(melhor_pos);
    det.cc.push_back(maior);

    // O canto superior esquerdo muda quando a escala muda; por isso comparamos
    // os CENTROS dos candidatos, e nao os cantos.
    int cx = melhor_pos.x + models[melhor_escala].cols / 2;
    int cy = melhor_pos.y + models[melhor_escala].rows / 2;

    // Apaga, em todas as escalas, os pixels a menos de 10 pixels do centro
    // do pico escolhido (raio ao quadrado < 100).
    for (int i = 0; i < n_escalas; i++) {
      int xt = cx - models[i].cols / 2;
      int yt = cy - models[i].rows / 2;
      for (int y = std::max(0, yt - 9); y <= std::min(cc[i].rows - 1, yt + 9); y++)
        for (int x = std::max(0, xt - 9); x <= std::min(cc[i].cols - 1, xt + 9); x++)
          if ((x-xt)*(x-xt) + (y-yt)*(y-yt) < 100) cc[i](y, x) = -1.0e30f;
    }
  }

  // -------------------------------------------------------------------------
  // (3) Calcula os mapas de NCC (correlacao normalizada) para todas as escalas.
  //
  // TM_CCOEFF_NORMED e' invariante a variacoes lineares de brilho/contraste,
  // por isso e' usado para CONFIRMAR se o candidato e' realmente a placa.
  // -------------------------------------------------------------------------
  std::vector<cv::Mat_<float>> ncc(n_escalas);
  #pragma omp parallel for
  for (int i = 0; i < n_escalas; i++)
    cv::matchTemplate(f, models[i], ncc[i], cv::TM_CCOEFF_NORMED);

  // -------------------------------------------------------------------------
  // (4) Consulta o NCC somente nas posicoes/escalas dos candidatos e guarda
  //     o candidato com maior NCC.
  // -------------------------------------------------------------------------
  float maior_ncc = -1.0f;
  for (int k = 0; k < (int)det.escala.size(); k++) {
    float v = ncc[det.escala[k]](det.pos[k].y, det.pos[k].x);
    det.ncc.push_back(v);
    if (v > maior_ncc) { maior_ncc = v; det.melhor = k; }
  }

  // A placa so e' considerada localizada se o melhor NCC passar do limiar.
  det.achou = (det.melhor >= 0 && maior_ncc > limiar);
  return det;
}

// Desenha na imagem d:
//   - todos os candidatos em azul (retangulo fino);
//   - o melhor candidato em amarelo (retangulo grosso), se a placa foi achada;
//   - os textos de escala/CC/NCC no topo e "Localizada"/"Nao Localizada".
void desenha_deteccao(cv::Mat_<COR>& d, const Modelos& mod, const Deteccao& det) {
  // Desenha todos os candidatos em azul.
  for (int k = 0; k < (int)det.escala.size(); k++)
    cv::rectangle(d, cv::Rect(det.pos[k].x, det.pos[k].y, mod.m[det.escala[k]].cols, mod.m[det.escala[k]].rows),
                  cv::Scalar(255,0,0), 1);

  if (det.melhor >= 0) {
    int k = det.melhor;

    // Destaca o melhor candidato em amarelo somente se ele foi confirmado.
    if (det.achou)
      cv::rectangle(d, cv::Rect(det.pos[k].x, det.pos[k].y, mod.m[det.escala[k]].cols, mod.m[det.escala[k]].rows),
                    cv::Scalar(0,255,255), 2);

    // Monta o texto de depuracao: escala, CC em % e NCC em %.
    std::string texto = "Escala: " + std::to_string(det.escala[k]) +
                        " CC: "  + std::to_string((int)std::round(det.cc[k]  * 100)) + "%" +
                        " NCC: " + std::to_string((int)std::round(det.ncc[k] * 100)) + "%";

    // Fundo preto no topo para o texto ficar legivel.
    cv::rectangle(d, cv::Rect(0, 0, d.cols, 25), cv::Scalar(0,0,0), cv::FILLED);
    cv::putText(d, texto, cv::Point(5,18), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0,255,255), 1);
  }

  // Status geral da deteccao (verde = localizada, vermelho = nao localizada).
  cv::putText(d, det.achou ? "Localizada" : "Nao Localizada", cv::Point(5,43),
              cv::FONT_HERSHEY_SIMPLEX, 0.45, det.achou ? cv::Scalar(0,255,0) : cv::Scalar(0,0,255), 1);
}

// ===========================================================================
// Preparo do digito
// ===========================================================================

// Retangulo (em pixels do quadro) onde esta o digito, dada a placa detectada.
//
// A janela foi medida no quadrado.png original (401x401). Se a placa no quadro
// esta na escala k, o lado do modelo e' mod.m[k].rows. A regra de tres abaixo
// converte as coordenadas da janela do modelo 401x401 para o tamanho real:
//
//   fator = lado_do_modelo / 401
//   x = pos.x + janela.x * fator + MARGEM_JANELA
//   ...
//
// Tambem cortamos o retangulo para nao sair do quadro (operador & de Rect).
cv::Rect janela_digito(const Modelos& mod, int escala, cv::Point pos, cv::Size tam_quadro) {
  double k = (double)mod.m[escala].rows / mod.lado_original;
  int x = pos.x + cvRound(mod.janela.x * k) + MARGEM_JANELA;
  int y = pos.y + cvRound(mod.janela.y * k) + MARGEM_JANELA;
  int w = cvRound(mod.janela.width  * k) - 2 * MARGEM_JANELA;
  int h = cvRound(mod.janela.height * k) - 2 * MARGEM_JANELA;

  // Janela pequena demais: nao da para ler o digito.
  if (w < 4 || h < 4) return cv::Rect();

  // Intersecao com o quadro: garante que x,y,w,h ficam dentro da imagem.
  return cv::Rect(x, y, w, h) & cv::Rect(0, 0, tam_quadro.width, tam_quadro.height);
}

// Passo 4b: ajuste de brilho/contraste. Fundo -> branco (255) e tinta -> preto (0).
// Devolve imagem vazia se nao ha contraste suficiente (janela em branco / sem digito).
cv::Mat_<uchar> ajusta_contraste(const cv::Mat_<uchar>& g) {
  // Otsu escolhe automaticamente um limiar que separa os pixels claros (fundo)
  // dos escuros (tinta). O resultado bin fica 255 no fundo e 0 na tinta.
  cv::Mat bin;
  cv::threshold(g, bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

  int n_fundo = cv::countNonZero(bin);              // quantidade de pixels claros
  int n_tinta = (int)bin.total() - n_fundo;         // quantidade de pixels escuros

  // Se um dos dois grupos estiver vazio, nao ha digito (ou a janela esta toda
  // branca / toda preta). Rejeitamos.
  if (n_fundo == 0 || n_tinta == 0) return cv::Mat_<uchar>();

  // inv e' a mascara da tinta (255 onde era escuro).
  cv::Mat inv; cv::bitwise_not(bin, inv);

  // Brilho medio do fundo e da tinta, para medir o contraste real.
  double fundo = cv::mean(g, bin)[0];
  double tinta = cv::mean(g, inv)[0];

  // Contraste muito baixo: provavelmente nao existe digito desenhado.
  if (fundo - tinta < CONTRASTE_MIN) return cv::Mat_<uchar>();

  // Mapa linear:
  //   tinta -> 0
  //   tinta + 75% do caminho ate o fundo -> 255
  // Tudo que estiver acima satura em 255; assim o fundo, mesmo com ruido,
  // fica branco puro (o bbox do MNIST precisa de branco puro para achar a
  // caixa da tinta).
  double alto = tinta + 0.75 * (fundo - tinta);
  cv::Mat_<uchar> saida;
  g.convertTo(saida, CV_8U, 255.0 / (alto - tinta), -tinta * 255.0 / (alto - tinta));
  return saida;
}

// ===========================================================================
// Classificador de digitos
// ===========================================================================

// Usa a classe MnistFlann do raspberry.hpp completo (FlaNN = vizinho mais
// proximo com FLANN). O FlaNN e' rapido para treinar e tem erro em torno de 3%
// no MNIST.
class ClassificadorDigitos {
  MnistFlann mnist;   // guarda as 60000 imagens de treino 14x14 e o indice FLANN

public:
  // Construtor:
  //   MnistFlann(14, true, true):
  //     - 14     = redimensiona cada imagem para 14x14;
  //     - true   = inverte as cores (fundo branco, tinta preta);
  //     - true   = ajusta o bounding box (remove linhas/colunas brancas).
  //   mnist.le(pasta, 60000, 0):
  //     - le somente os 60000 exemplos de treino;
  //     - nq=0 faz pular os 10000 exemplos de teste (nao precisamos deles aqui).
  //   mnist.train():
  //     - constroi o indice FLANN (4 arvores kd) sobre a matriz ax.
  ClassificadorDigitos(const std::string& pasta) : mnist(TAM_DIGITO, true, true) {
    mnist.le(pasta, 60000, 0);
    mnist.train();
  }

  // Passo 4c: a = imagem float 0..1, tinta escura (<=0.5) sobre fundo branco.
  // Devolve o digito recortado pela caixa da tinta e reduzido a 14x14, ou
  // vazia se nao ha tinta.
  cv::Mat_<float> normaliza_digito(const cv::Mat_<float>& a) {
    cv::Mat_<float> b = mnist.bbox(a);          // versao FLT de bbox
    if (!mnist.localizou) return cv::Mat_<float>();
    return b;
  }

  // Passo 4d: b = digito 14x14 ja normalizado. Devolve 0..9.
  int classifica(const cv::Mat_<float>& b) {
    // O FLANN espera uma consulta por linha. Cada imagem 14x14 vira uma linha
    // de 196 valores (14*14), na mesma ordem linha a linha usada no treino.
    cv::Mat_<float> q(1, b.rows * b.cols);
    for (int l = 0; l < b.rows; l++)
      for (int c = 0; c < b.cols; c++)
        q(0, l * b.cols + c) = b(l, c);

    // predictInterno devolve o rotulo (0..9) do vizinho mais proximo.
    return cvRound(mnist.predictInterno(q));
  }
};

// ===========================================================================
// Leitura do digito de um quadro
// ===========================================================================

// Resultado da leitura:
//   digito  = -1 se nao leu; 0..9 se leu;
//   recorte = passo 4a: recorte colorido da janela (para mostrar na tela);
//   d14     = passo 4c: digito 14x14 em uchar (para mostrar no painel).
struct LeituraDigito {
  int digito = -1;
  cv::Mat_<COR>   recorte;
  cv::Mat_<uchar> d14;
};

// So le se a placa foi achada e estiver perto (escala <= ESCALA_PERTO).
LeituraDigito le_digito(const cv::Mat_<COR>& a, const Modelos& mod, const Deteccao& det,
                        ClassificadorDigitos& clf) {
  LeituraDigito r;

  // Sem placa ou placa longe: nao tenta ler.
  if (!det.achou) return r;
  int k = det.melhor;
  if (det.escala[k] > ESCALA_PERTO) return r;

  // Calcula a janela do digito no quadro atual.
  cv::Rect j = janela_digito(mod, det.escala[k], det.pos[k], a.size());
  if (j.empty()) return r;

  // Passo 4a: copia a imagem colorida do digito para uma nova imagem.
  r.recorte = a(j).clone();

  // Passo 4b: converte para cinza e ajusta brilho/contraste.
  cv::Mat_<uchar> cinza;
  cv::cvtColor(r.recorte, cinza, cv::COLOR_BGR2GRAY);
  cv::Mat_<uchar> ajustada = ajusta_contraste(cinza);
  if (ajustada.empty()) return r;

  // Converte para float 0..1, porque o bbox FLT do MNIST trabalha nessa faixa.
  cv::Mat_<float> af;
  ajustada.convertTo(af, CV_32F, 1.0 / 255.0);

  // Passo 4c: bbox + redimensionamento para 14x14.
  cv::Mat_<float> b = clf.normaliza_digito(af);
  if (b.empty()) return r;

  // Copia o 14x14 para uchar so para desenhar no painel de depuracao.
  b.convertTo(r.d14, CV_8U, 255.0);

  // Passo 4d: classifica com o FlaNN.
  r.digito = clf.classifica(b);
  return r;
}

// Painel lateral (como na figura 1b do enunciado):
//   - em cima: o digito 14x14 ampliado (passo 4c);
//   - embaixo: o recorte colorido da janela (passo 4a);
//   - em vermelho: o digito lido (0..9).
//
// A ampliacao usa INTER_NEAREST para que o professor/voce veja os pixels
// reais do 14x14, sem suavizacao.
cv::Mat_<COR> monta_painel(const cv::Mat_<COR>& recorte, const cv::Mat_<uchar>& d14, int digito, int altura) {
  // Largura do painel. 120 faz o video de saida ficar 320+120 = 440 pixels
  // de largura, igual ao locarec.avi de referencia do professor.
  const int L = 120;

  // Cria o painel cinza (fundo neutro).
  cv::Mat_<COR> painel(altura, L, COR(128,128,128));

  // Desenha o digito 14x14 ampliado no topo.
  if (!d14.empty()) {
    cv::Mat_<uchar> g;
    cv::resize(d14, g, cv::Size(L, L), 0, 0, cv::INTER_NEAREST);
    cv::Mat_<COR> c;
    cv::cvtColor(g, c, cv::COLOR_GRAY2BGR);
    c.copyTo(painel(cv::Rect(0, 0, L, L)));
  }

  // Desenha o recorte colorido embaixo.
  if (!recorte.empty()) {
    cv::Mat_<COR> r;
    cv::resize(recorte, r, cv::Size(L, L), 0, 0, cv::INTER_NEAREST);
    r.copyTo(painel(cv::Rect(0, L, L, L)));
  }

  // Escreve o digito lido em vermelho sobre o recorte.
  if (digito >= 0)
    cv::putText(painel, std::to_string(digito), cv::Point(25, L + 60),
                cv::FONT_HERSHEY_SIMPLEX, 2.0, cv::Scalar(0,0,255), 3);

  return painel;
}
