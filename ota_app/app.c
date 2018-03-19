#include <string.h>
#include <ti/drivers/PWM.h>
#include <ti/sysbios/knl/Task.h>

#include <../boards/_CC1350_LAUNCHXL/_Board.h>

#include "Include/ota.h"

int payload_data = 4;
static char payload_string[] = "Th1s is a string";
static const char *payload_literal = "This is a string literal";

void payload_test_app(UArg arg1, UArg arg2) {
    PWM_Handle pwm;
    PWM_Params pwm_p;
    PWM_init();
    PWM_Params_init(&pwm_p);
    pwm_p.dutyUnits = PWM_DUTY_US;
    pwm_p.dutyValue = 100;
    pwm_p.periodUnits = PWM_PERIOD_US;
    pwm_p.periodValue = 100;
    pwm = PWM_open(Board_PWM0, &pwm_p);
    while (pwm == NULL)
        ;
    PWM_start(pwm);
    payload_data = strlen(payload_string) + strlen(payload_literal);
}
DEFINE_ENTRYPOINY(payload_test_app);

int payload_test_app_end(void) {
    char c = 1;
    if (c)
        return 1;
    return 0;
}
