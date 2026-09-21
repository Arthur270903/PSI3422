#if defined(CONFIG_APP_ROLE_TRANSMITTER)

#include <zephyr/kernel.h>

#include "bridge_h.h"
#include "encoder.h"
#include "ultrasound.h"
#include "radio_link.h"
#include "calibration.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_car_control, LOG_LEVEL_INF);

K_MSGQ_DEFINE(command_msgq, sizeof(command_t), 4, 4);
K_SEM_DEFINE(target_sem,            0, 1);
K_SEM_DEFINE(turn_start_sem,        0, 1);
K_SEM_DEFINE(turn_done_sem,         0, 1);
K_SEM_DEFINE(rotation_complete_sem, 0, 1);

/* pulsos_90 = (bitola * pulsos_por_volta) / (4 * diametro_roda)
 * -> quantos pulsos cada roda precisa girar (em sentidos opostos)
 *    para o carrinho fazer um giro de 90 graus no proprio eixo.
 * Isso e uma aproximacao geometrica (sem escorregamento); se na pratica
 * o giro passar ou nao completar 90 graus, ajuste esse valor na mao. */
#define PULSES_90_DEGREES ((CONFIG_APP_WHEELBASE * CONFIG_APP_PULSES_PER_TURN) / (4 * CONFIG_APP_WHELL_DIAMETER))

typedef enum 
{
    STATE_MOVING_FORWARD,
    STATE_AVOIDING_OBSTACLE,
    STATE_STOP
} car_state_t;

typedef enum 
{ 
    TURN_LEFT, 
    TURN_RIGHT 
} turn_dir_t;

struct rotation_command 
{
    turn_dir_t dir;
    uint16_t angle; 
} turn_command;

void rotation_thread_entry(void *p1, void *p2, void *p3) 
{
    while (1) 
    {
        k_sem_take(&turn_start_sem, K_FOREVER);

        uint32_t target = (turn_command.angle * ((CONFIG_APP_WHEELBASE * CONFIG_APP_PULSES_PER_TURN) / (4 * CONFIG_APP_WHELL_DIAMETER))) / 90; 

        LOG_INF("VALUE: %d", target);
        encoder_reset();
        encoder_set_target_notify(target, &rotation_complete_sem);

        if (turn_command.dir == TURN_RIGHT)
            bridge_h_right();
        else
            bridge_h_left();

        k_sem_take(&rotation_complete_sem, K_FOREVER);

        bridge_h_stop();

        k_sem_give(&turn_done_sem);
    }
}

void request_rotation(turn_dir_t dir, uint16_t angle) 
{
    turn_command.dir = dir;
    turn_command.angle = angle;

    k_sem_give(&turn_start_sem);
    k_sem_take(&turn_done_sem, K_FOREVER);
}

bool check_obstacle() 
{
    return !(k_sem_take(&target_sem, K_NO_WAIT));
}

void motor_control_entry_point(void *, void *, void *)
{ 
    encoders_init();
    encoder_reset();

    if(bridge_h_init())
        return;

    if(motor_calibrate())
        return;

    ultrasound_set_threshold_notify(CONFIG_APP_TARGET_DISTANCE_CM, &target_sem);

    car_state_t car_state = STATE_MOVING_FORWARD;
    command_t current_command = COMMAND_STOP;

    struct k_poll_event events[2] = {
        K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
                                        K_POLL_MODE_NOTIFY_ONLY,
                                        &target_sem, 0),
        K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
                                        K_POLL_MODE_NOTIFY_ONLY,
                                        &command_msgq, 0),
    };

    while (1) 
    {   
        switch(car_state) 
        {
            case STATE_MOVING_FORWARD:
                LOG_INF("STATE_MOVING_FORWARD");
                k_sem_reset(&target_sem); 
                bridge_h_front();

                events[0].state = K_POLL_STATE_NOT_READY;
                events[1].state = K_POLL_STATE_NOT_READY;

                k_poll(events, ARRAY_SIZE(events), K_FOREVER);

                if (events[0].state == K_POLL_STATE_SEM_AVAILABLE)
                {
                    k_sem_take(&target_sem, K_NO_WAIT);
                    car_state = STATE_AVOIDING_OBSTACLE;
                }

                if (events[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE)
                {
                    k_msgq_get(&command_msgq, &current_command, K_NO_WAIT);

                    if (current_command == COMMAND_STOP)
                        car_state = STATE_STOP;
                }
                break;

                case STATE_AVOIDING_OBSTACLE:
                    LOG_INF("STATE_AVOIDING_OBSTACLE");
                    bridge_h_stop();
                    k_msleep(CONFIG_APP_DOWNTIME_MS);

                    request_rotation(TURN_RIGHT, 90);
                    k_sem_reset(&target_sem);
                    k_msleep(CONFIG_APP_DOWNTIME_MS);

                    if (check_obstacle()) 
                    {
                        request_rotation(TURN_LEFT, 180);
                        k_sem_reset(&target_sem);
                        k_msleep(CONFIG_APP_DOWNTIME_MS);

                        if (check_obstacle()) 
                        {
                            request_rotation(TURN_LEFT, 90);
                            bridge_h_stop();
                        }
                    }

                    k_sem_reset(&target_sem);
                    events[0].state = K_POLL_STATE_NOT_READY;
                    car_state = STATE_MOVING_FORWARD;
                    break;
            
            case STATE_STOP:
                LOG_INF("STATE_AVSTATE_STOPOIDING_OBSTACLE");
                bridge_h_stop();
                k_msgq_get(&command_msgq, &current_command, K_FOREVER);

                if (current_command == COMMAND_RUN)
                    car_state = STATE_MOVING_FORWARD;
                break;
        }
    }
}


K_THREAD_DEFINE(rotation_thread_id, 1024, 
                rotation_thread_entry, 
                NULL, NULL, NULL, 
                5, 0, 0);

K_THREAD_DEFINE(motor_control_tid, 512,
                motor_control_entry_point, 
                NULL, NULL, NULL,
                0, 0, 0);
#endif /* defined(CONFIG_APP_ROLE_TRANSMITTER) */