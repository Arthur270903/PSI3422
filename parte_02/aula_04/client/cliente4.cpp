//cliente4.cpp
//Computador$ compila cliente4 -ocv -v3 -omp
//Uso: cliente4 192.168.0.110 [videosaida.avi] [t/c]   (quadrado.png na pasta atual)
#include <omp.h>
#include "../projeto.hpp"

constexpr int NL = 240, NC = 320;     // tamanho do quadro
constexpr int NESCALAS = 10;
constexpr int BASE  = 40;             // velocidade para frente (-100..100)
constexpr int GANHO = 40;             // diferenca de velocidade quando a placa esta na borda
constexpr int ESCALA_PERTO = 1;       // escala <= 1: placa grande = camera muito perto

int main(int argc, char *argv[]) {
  if (argc < 2) erro("cliente4 IP [videosaida.avi] [t/c]\n");
  bool gravar = (argc >= 3);
  char modo = (argc >= 4) ? argv[3][0] : 't';

  // ---- Modelos (igual a fase3p) ----
  Mat_<COR> temp = imread("assets/quadrado.png", IMREAD_COLOR);
  if (temp.empty()) erro("Falha na leitura de quadrado.png");
  Mat_<float> T;
  converte(temp, T);
  vector<Mat_<float>> models(NESCALAS);
  for (int i = 0; i < NESCALAS; i++) {
    float scale = 0.1721f * pow(0.0473f / 0.1721f, i / 9.0f);
    int side = round(T.rows * scale);
    Mat_<float> resized;
    resize(T, resized, Size(side, side), 0, 0, INTER_NEAREST);
    models[i] = somaAbsDois(dcReject(resized, 1.0f));
  }

  CLIENT client(argv[1]);
  Mat_<COR> a, d;
  Mat_<float> f;
  VideoWriter vo;
  namedWindow("janela");

  BYTE cmd[3] = {'0', 100, 100};      // cmd, velocidade esq+100, velocidade dir+100
  client.sendBytes(3, cmd);           // pede o primeiro quadro (parado)
  while (true) {
    client.receiveImgComp(a);
    converte(a, f);

    // ---- Mapas CC das 10 escalas ----
    vector<Mat_<float>> cc(NESCALAS);
    #pragma omp parallel for
    for (int i = 0; i < NESCALAS; i++)
      matchTemplate(f, models[i], cc[i], TM_CCORR);

    // ---- 20 maiores picos de CC, separados por pelo menos 10 pixels ----
    vector<int> cand_scale;
    vector<Point> cand_pos;
    vector<float> cand_cc;
    for (int k = 0; k < 20; k++) {
      float largest = -1.0e30f;
      int best_scale = -1;
      Point best_pos;
      for (int i = 0; i < NESCALAS; i++) {
        double mx; Point p;
        minMaxLoc(cc[i], nullptr, &mx, nullptr, &p);
        if (mx > largest) { largest = mx; best_scale = i; best_pos = p; }
      }
      if (best_scale == -1 || largest <= -1.0e29) break;
      cand_scale.push_back(best_scale);
      cand_pos.push_back(best_pos);
      cand_cc.push_back(largest);

      int cx = best_pos.x + models[best_scale].cols / 2;
      int cy = best_pos.y + models[best_scale].rows / 2;
      for (int i = 0; i < NESCALAS; i++) {
        int xt = cx - models[i].cols / 2;
        int yt = cy - models[i].rows / 2;
        for (int y = max(0, yt - 9); y <= min(cc[i].rows - 1, yt + 9); y++)
          for (int x = max(0, xt - 9); x <= min(cc[i].cols - 1, xt + 9); x++)
            if ((x-xt)*(x-xt) + (y-yt)*(y-yt) < 100) cc[i](y, x) = -1.0e30f;
      }
    }

    // ---- NCC so nos candidatos ----
    vector<Mat_<float>> ncc(NESCALAS);
    #pragma omp parallel for
    for (int i = 0; i < NESCALAS; i++)
      matchTemplate(f, models[i], ncc[i], TM_CCOEFF_NORMED);

    int best = -1;
    float largest_ncc = -1.0f;
    for (int k = 0; k < (int)cand_scale.size(); k++) {
      float v = ncc[cand_scale[k]](cand_pos[k].y, cand_pos[k].x);
      if (v > largest_ncc) { largest_ncc = v; best = k; }
    }
    bool find = (best >= 0 && largest_ncc > 0.55f);

    // ---- Controle: so usa a posicao x ----
    int esq = 0, dir = 0;             // parado por padrao
    string status = "Nao localizada (parado)";
    if (find) {
      int scale = cand_scale[best];
      if (scale <= ESCALA_PERTO) {
        status = "Muito perto (parado)";
      } else {
        float cx = cand_pos[best].x + models[scale].cols / 2.0f;
        float erro = (cx - NC / 2.0f) / (NC / 2.0f);   // -1 (esquerda) .. +1 (direita)
        esq = BASE + GANHO * erro;    // placa a direita: roda esquerda mais rapida
        dir = BASE - GANHO * erro;
        status = "Seguindo";
      }
    }

    // ---- Tela de debug ----
    a.copyTo(d);
    for (int k = 0; k < (int)cand_scale.size(); k++)
      rectangle(d, Rect(cand_pos[k].x, cand_pos[k].y, models[cand_scale[k]].cols, models[cand_scale[k]].rows),
                Scalar(255, 0, 0), 1);
    if (find)
      rectangle(d, Rect(cand_pos[best].x, cand_pos[best].y, models[cand_scale[best]].cols, models[cand_scale[best]].rows),
                Scalar(0, 255, 255), 2);
    putText(d, status, Point(5, 18), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 1);
    putText(d, "E=" + to_string(esq) + " D=" + to_string(dir), Point(5, 38),
            FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 1);
    imshow("janela", d);

    if (gravar) {
      Mat saida = (modo == 'c') ? Mat(a) : Mat(d);
      if (!vo.isOpened())
        vo.open(argv[2], VideoWriter::fourcc('M','J','P','G'), 15, saida.size());
      vo << saida;
    }

    int ch = waitKey(1) & 0xff;
    if (ch == 27) { cmd[0] = 's'; client.sendBytes(3, cmd); break; }
    cmd[0] = '0';
    cmd[1] = esq + 100;
    cmd[2] = dir + 100;
    client.sendBytes(3, cmd);
  }
}
