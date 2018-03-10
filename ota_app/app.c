#include <ti/drivers/PWM.h>
#include <ti/sysbios/knl/Task.h>

#include <../boards/_CC1350_LAUNCHXL/_Board.h>

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
}

int payload_test_app_end(void) {
    char c = 1;
    if (c)
        return 1;
    return 0;
}
