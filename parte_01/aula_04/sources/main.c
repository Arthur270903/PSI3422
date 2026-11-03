#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

/*
 * HW-201 como encoder: contamos AMBAS as bordas (GPIO_INT_EDGE_BOTH),
 * entao 1 volta completa = 48 pulsos (24 faixas escuras + 24 claras).
 */
#define PULSOS_POR_VOLTA (48)

/* AJUSTE estes dois valores com as medidas reais do seu carrinho.
 * Os numeros abaixo sao só um ponto de partida. */
#define DIAMETRO_RODA_MM (66)   // diametro da roda, em mm
#define BITOLA_MM        (120)  // distancia entre o centro das duas rodas, em mm

/* pulsos_90 = (bitola * pulsos_por_volta) / (4 * diametro_roda)
 * -> quantos pulsos cada roda precisa girar (em sentidos opostos)
 *    para o carrinho fazer um giro de 90 graus no proprio eixo.
 * Isso e uma aproximacao geometrica (sem escorregamento); se na pratica
 * o giro passar ou nao completar 90 graus, ajuste esse valor na mao. */
#define PULSOS_90_GRAUS ((BITOLA_MM * PULSOS_POR_VOLTA) / (4 * DIAMETRO_RODA_MM))

#define DELAY_ENTRE_ACOES_MS (10000)

/* 1 = modo alinhamento (so mostra transicoes dos encoders, sem mexer nos motores)
 * 0 = modo movimento (executa a sequencia de giros)
 */
#define MODO_ALINHAMENTO (0)

static const struct gpio_dt_spec encoder_esquerda = GPIO_DT_SPEC_GET(DT_ALIAS(irsensor0), gpios);
static const struct gpio_dt_spec encoder_direita  = GPIO_DT_SPEC_GET(DT_ALIAS(irsensor1), gpios);

static const struct gpio_dt_spec motor_in1 = GPIO_DT_SPEC_GET(DT_ALIAS(motorin1), gpios);
static const struct gpio_dt_spec motor_in2 = GPIO_DT_SPEC_GET(DT_ALIAS(motorin2), gpios);
static const struct gpio_dt_spec motor_in3 = GPIO_DT_SPEC_GET(DT_ALIAS(motorin3), gpios);
static const struct gpio_dt_spec motor_in4 = GPIO_DT_SPEC_GET(DT_ALIAS(motorin4), gpios);

#if MODO_ALINHAMENTO

int main(void)
{
    if (!gpio_is_ready_dt(&encoder_esquerda) || !gpio_is_ready_dt(&encoder_direita))
    {
        printk("Erro: sensores nao estao prontos\n");
        return 0;
    }

    gpio_pin_configure_dt(&encoder_esquerda, GPIO_INPUT);
    gpio_pin_configure_dt(&encoder_direita, GPIO_INPUT);

    int estado_esq_anterior = -1;
    int estado_dir_anterior = -1;

    printk("--- alinhamento dos encoders ---\n");

    while (1)
    {
        int estado_esq = gpio_pin_get_dt(&encoder_esquerda);
        int estado_dir = gpio_pin_get_dt(&encoder_direita);

        if (estado_esq != estado_esq_anterior)
        {
            printk("Encoder esquerda (PTA4): %s\n", estado_esq ? "MARCA DETECTADA" : "livre");
            estado_esq_anterior = estado_esq;
        }

        if (estado_dir != estado_dir_anterior)
        {
            printk("Encoder direita (PTA5): %s\n", estado_dir ? "MARCA DETECTADA" : "livre");
            estado_dir_anterior = estado_dir;
        }

        k_msleep(5);
    }

    return 0;
}

#else /* modo movimento */

static struct gpio_callback encoder_cb_data;

static volatile uint32_t pulsos_esquerda = 0;
static volatile uint32_t pulsos_direita = 0;

/* Debounce curto: filtra o "chatter" eletrico do comparador LM393 quando o
 * sensor fica numa distancia ambigua (ex.: bloqueado por um objeto), sem
 * descartar pulsos reais do disco (que costumam vir vários ms distantes
 * um do outro, mesmo em rotacao alta). Ajuste se necessario. */
#define DEBOUNCE_MS (2)

static int64_t ultimo_pulso_esquerda = 0;
static int64_t ultimo_pulso_direita = 0;

static void encoder_irq_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    int64_t agora = k_uptime_get();

    if ((pins & BIT(encoder_esquerda.pin)) && (agora - ultimo_pulso_esquerda) >= DEBOUNCE_MS)
    {
        pulsos_esquerda++;
        ultimo_pulso_esquerda = agora;
    }

    if ((pins & BIT(encoder_direita.pin)) && (agora - ultimo_pulso_direita) >= DEBOUNCE_MS)
    {
        pulsos_direita++;
        ultimo_pulso_direita = agora;
    }
}

/* --- controle dos motores (L298N, sem PWM: ENA/ENB fixos via jumper) --- */

static void motor_esquerda_frente(void)
{
    gpio_pin_set_dt(&motor_in1, 1);
    gpio_pin_set_dt(&motor_in2, 0);
}

static void motor_esquerda_re(void)
{
    gpio_pin_set_dt(&motor_in1, 0);
    gpio_pin_set_dt(&motor_in2, 1);
}

static void motor_esquerda_parar(void)
{
    gpio_pin_set_dt(&motor_in1, 0);
    gpio_pin_set_dt(&motor_in2, 0);
}

static void motor_direita_frente(void)
{
    gpio_pin_set_dt(&motor_in3, 1);
    gpio_pin_set_dt(&motor_in4, 0);
}

static void motor_direita_re(void)
{
    gpio_pin_set_dt(&motor_in3, 0);
    gpio_pin_set_dt(&motor_in4, 1);
}

static void motor_direita_parar(void)
{
    gpio_pin_set_dt(&motor_in3, 0);
    gpio_pin_set_dt(&motor_in4, 0);
}

/* --- giro no proprio eixo, controlado pelos encoders --- */

typedef enum {
    GIRAR_ESQUERDA,
    GIRAR_DIREITA,
} direcao_giro_t;

static void girar_90_graus(direcao_giro_t direcao)
{
    pulsos_esquerda = 0;
    pulsos_direita = 0;

    if (direcao == GIRAR_ESQUERDA)
    {
        printk("Girando 90 graus para a ESQUERDA...\n");
        motor_esquerda_re();
        motor_direita_frente();
    }
    else
    {
        printk("Girando 90 graus para a DIREITA...\n");
        motor_esquerda_frente();
        motor_direita_re();
    }

    /* espera ate a media dos dois encoders atingir o alvo calculado */
    while (((pulsos_esquerda + pulsos_direita) / 2) < PULSOS_90_GRAUS)
    {
        k_msleep(2);
    }

    motor_esquerda_parar();
    motor_direita_parar();

    printk("Giro concluido (esquerda: %u pulsos | direita: %u pulsos | alvo: %u)\n",
           pulsos_esquerda, pulsos_direita, PULSOS_90_GRAUS);
}

int main(void)
{
    if (!gpio_is_ready_dt(&encoder_esquerda) || !gpio_is_ready_dt(&encoder_direita) ||
        !gpio_is_ready_dt(&motor_in1) || !gpio_is_ready_dt(&motor_in2) ||
        !gpio_is_ready_dt(&motor_in3) || !gpio_is_ready_dt(&motor_in4))
    {
        printk("Erro: sensores ou pinos do motor nao estao prontos\n");
        return 0;
    }

    gpio_pin_configure_dt(&encoder_esquerda, GPIO_INPUT);
    gpio_pin_configure_dt(&encoder_direita, GPIO_INPUT);

    /* conta as duas bordas: 48 pulsos por volta, como o disco tem 48 partes */
    gpio_pin_interrupt_configure_dt(&encoder_esquerda, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure_dt(&encoder_direita, GPIO_INT_EDGE_BOTH);

    gpio_init_callback(&encoder_cb_data, encoder_irq_handler,
                        BIT(encoder_esquerda.pin) | BIT(encoder_direita.pin));

    /* mesmo device (gpioa) para os dois pinos, um add_callback basta */
    gpio_add_callback(encoder_esquerda.port, &encoder_cb_data);

    gpio_pin_configure_dt(&motor_in1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&motor_in2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&motor_in3, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&motor_in4, GPIO_OUTPUT_INACTIVE);

    printk("--- movimento: esquerda / direita / direita / esquerda (loop) ---\n");
    printk("Alvo por giro: %u pulsos\n", PULSOS_90_GRAUS);

    direcao_giro_t sequencia[] = { GIRAR_ESQUERDA, GIRAR_DIREITA, GIRAR_DIREITA, GIRAR_ESQUERDA };
    size_t indice = 0;

    while (1)
    {
        girar_90_graus(sequencia[indice]);
        indice = (indice + 1) % ARRAY_SIZE(sequencia);

        k_msleep(DELAY_ENTRE_ACOES_MS);
    }

    return 0;
}

#endif