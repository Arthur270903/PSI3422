Atividade 05
============

No arquivo ``fsl_tpm.h``, localizado em  ``~/.platformio/packages/framework-zephyr/_pio/modules/hal/nxp/mcux/mcux-sdk-ng/drivers/tpm/fsl_tpm.h``, adicionar nas primeiras linhas, após os ``#include``, o seguinte bloco de código:

.. code-block:: c

    #ifndef FSL_FEATURE_TPM_HAS_32BIT_COUNTERn
    #define FSL_FEATURE_TPM_HAS_32BIT_COUNTERn(x) (0)
    #endif

No arquivo ``pwm_mcux_tpm.c``, localizado em  ``~/.platformio/packages/framework-zephyr/drivers/pwm/pwm_mcux_tpm.c``, corrigir o erro, na linha **64**, da variável ``base`` não definida, dentro da função ``mcux_tpm_set_cycles``, com o seguinte código:

.. code-block:: c

    if (period_cycles == 0 || period_cycles == TPM_MAX_COUNTER_VALUE(base)) {


Para:

.. code-block:: c

    if (period_cycles == 0 || period_cycles == TPM_MAX_COUNTER_VALUE(config->base)) {
