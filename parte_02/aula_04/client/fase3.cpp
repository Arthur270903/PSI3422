// g++ fase3.cpp -O3 `pkg-config --cflags --libs opencv4` 
#include <opencv2/opencv.hpp>
#include <iostream>

#include "../raspberry.hpp"

int main(int argc, char** argv) 
{
    if (argc != 4)
    {
        std::cout << "Uso: fase3 video_entrada.avi quadrado.png video_saida.avi" << std::endl;
        return -1;
    }

    constexpr int number_of_lines   = 240;                        //
    constexpr int number_of_columns = 320;                        // 0 = preto
    cv::Mat_<Vec3b> temp = cv::imread(argv[2], cv::IMREAD_COLOR); // 1 = branco

    if (temp.empty()) 
    {
        std::cerr << "Erro: Falha na leitura quadrado.png" << std::endl;
        exit(-1);
    }

    Mat_<float> T; 
    converte(temp, T); // size(T) = 401x401

    // Gera os 10 modelos em progressao geometrica.
    // Cada modelo e redimensionado e recebe o pre-processamento don't care
    constexpr int number_of_scales = 10;

    std::vector<cv::Mat_<float>> models(number_of_scales);

    for (int i = 0; i < number_of_scales; i++)
    {
        float scale = 0.1721f * std::pow(0.0473f / 0.1721f, i / 9.0f);

        int side = std::round(T.rows * scale);

        cv::Mat_<float> resized;
        cv::resize(T, resized, cv::Size(side, side), 0, 0, cv::INTER_NEAREST);

        cv::Mat_<float> T2 = somaAbsDois(dcReject(resized, 1.0f));

        models[i] = T2;
    }

    // Abre o video de entrada
    cv::VideoCapture video_capture(argv[1]); 
    if (!video_capture.isOpened()) 
    {
        std::cerr << "Erro: Falha na abertura de video"<< std::endl;
        exit(-1);
    }
    // Cria o video de saida.
    cv::VideoWriter video_writer(argv[3], cv::VideoWriter::fourcc('X','V','I','D'), 20, cv::Size(number_of_columns, number_of_lines));
    if (!video_writer.isOpened())
    {
        std::cerr << "Erro: Falha na criacao do video de saida" << std::endl;
        return -1;
    }

    cv::Mat_<Vec3b> a, d;
    cv::Mat_<float> f;

    int frame_count = 0;
    auto init_count = std::chrono::steady_clock::now();

    while (true) 
    {
        // w.grab(); // se quiser limpar buffer da camera
        video_capture >> a; // pega um quadro do video
        if (!a.data)
            break;

        if (a.rows != number_of_lines || a.cols != number_of_columns)
        {
            std::cerr << "Erro: O video deve possuir quadros de 320x240 pixels" << std::endl;
            return -1;
        }

        converte(a, f);

        // Calcula os mapas de correlacao CC para as 10 escalas.
        std::vector<Mat_<float>> cc(number_of_scales);

        for (int i = 0; i < number_of_scales; i++)
            matchTemplate(f, models[i], cc[i], TM_CCORR);

        // Encontra os 20 maiores picos de CC, separados por pelo menos 10 pixels.
        std::vector<int>       candidate_scale;
        std::vector<cv::Point> candidate_position;
        std::vector<float>     candidate_cc;

        for (int k = 0; k < 20; k++)
        {
            float     largest_cc = -1.0e30f;
            int       best_scale = -1;
            cv::Point best_position;

            // Procura o maior valor entre os mapas de todas as escalas.
            for (int i = 0; i < number_of_scales; i++)
            {
                double max;
                Point position;
                cv::minMaxLoc(cc[i], nullptr, &max, nullptr, &position);

                if (max > largest_cc)
                {
                    largest_cc = max;
                    best_scale = i;
                    best_position = position;
                }
            }

            if (best_scale == -1 || largest_cc <= -1.0e29)
                break;

            candidate_scale.push_back(best_scale);
            candidate_position.push_back(best_position);
            candidate_cc.push_back(largest_cc);

            // Compara os centros dos modelos, pois o canto superior esquerdo
            // muda quando a escala muda.
            int center_x = best_position.x + models[best_scale].cols / 2;
            int center_y = best_position.y + models[best_scale].rows / 2;

            // Descarta, em todas as escalas, picos a menos de 10 pixels
            // do centro do pico encontrado.
            for (int i = 0; i < number_of_scales; i++)
            {
                int x_temp = center_x - models[i].cols / 2;
                int y_temp = center_y - models[i].rows / 2;

                for (int y = std::max(0, y_temp - 9);
                     y <= std::min(cc[i].rows - 1, y_temp + 9); y++)
                {
                    for (int x = std::max(0, x_temp - 9);
                         x <= std::min(cc[i].cols - 1, x_temp + 9); x++)
                    {
                        int dx = x - x_temp;
                        int dy = y - y_temp;

                        if (dx * dx + dy * dy < 100)
                            cc[i](y, x) = -1.0e30f;
                    }
                }
            }
        }

        // Calcula NCC para cada escala.
        std::vector<Mat_<float>> ncc(number_of_scales);

        for (int i = 0; i < number_of_scales; i++)
            matchTemplate(f, models[i], ncc[i], TM_CCOEFF_NORMED);

        // Consulta o NCC somente nas posicoes dos 20 candidatos.
        std::vector<float> candidate_ncc(candidate_scale.size());
        int best_candidate = -1;
        float largest_ncc = -1.0f;

        for (int k = 0; k < (int)candidate_scale.size(); k++)
        {
            int scale = candidate_scale[k];
            int x = candidate_position[k].x;
            int y = candidate_position[k].y;

            candidate_ncc[k] = ncc[scale](y, x);

            if (candidate_ncc[k] > largest_ncc)
            {
                largest_ncc = candidate_ncc[k];
                best_candidate = k;
            }
        }

        // Verifica se o maior NCC ultrapassa o limite de 0,55.
        bool find = (best_candidate >= 0 && largest_ncc > 0.55f);

        // Copia o quadro original.
        a.copyTo(d);

        // Desenha os 20 candidatos em azul.
        for (int k = 0; k < (int)candidate_scale.size(); k++)
        {
            int scale = candidate_scale[k];
            int x = candidate_position[k].x;
            int y = candidate_position[k].y;

            rectangle(d, Rect(x, y, models[scale].cols, models[scale].rows),
                    Scalar(255, 0, 0), 1);
        }

        // Destaca o melhor candidato em amarelo.
        if (best_candidate >= 0)
        {
            int scale = candidate_scale[best_candidate];
            int x = candidate_position[best_candidate].x;
            int y = candidate_position[best_candidate].y;
            
            if (find)
                rectangle(d, Rect(x, y, models[scale].cols, models[scale].rows),
                        Scalar(0, 255, 255), 2);

            // Informacoes do melhor candidato.
            std::string text =
                "Escala: " + std::to_string(scale) +
                " CC: " + std::to_string((int)round(candidate_cc[best_candidate] * 100)) + "%" +
                " NCC: " + std::to_string((int)round(largest_ncc * 100)) + "%";

            // Fundo preto para facilitar a leitura.
            rectangle(d, Rect(0, 0, number_of_columns, 25),
                    Scalar(0, 0, 0), FILLED);

            putText(d, text, Point(5, 18),
                    FONT_HERSHEY_SIMPLEX, 0.45,
                    Scalar(0, 255, 255), 1);
        }

        // Indica se a placa foi localizada.
        std::string status = find ? "Localizada" : "Nao Localizada";

        putText(d, status, Point(5, 43),
                FONT_HERSHEY_SIMPLEX, 0.45,
                find ? Scalar(0, 255, 0) : Scalar(0, 0, 255), 1);

        video_writer << d; // salva o quadro no video de saida
        frame_count++;
        //if (frame_count % 10 == 0)
        //    std::cout << "frame: " << frame_count << std::endl;
    }

    auto end_count = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_count - init_count).count();

    std::cout << "fps: " << frame_count / elapsed << std::endl;

    return 0;
}


