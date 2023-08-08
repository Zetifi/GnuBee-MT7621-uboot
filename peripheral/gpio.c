/*************************************************************************
	> File Name: gpio.c
	> Author:
	> Mail:
	> Created Time: 2015年04月11日 星期六 15时22分19秒
 ************************************************************************/
#include <gpio.h>

#define GPIO_MODE_REG (*(volatile unsigned int *)0x1E000060)
#define GPIO_CTRL_0_REG (*(volatile unsigned int *)0x1E000600) // (GPIO#0--GPIO#31)
#define GPIO_CTRL_1_REG (*(volatile unsigned int *)0x1E000604) // (GPIO#32-GPIO#63)
#define GPIO_DATA_0_REG (*(volatile unsigned int *)0x1E000620)
#define GPIO_DATA_1_REG (*(volatile unsigned int *)0x1E000624)

#define RGMII2_MODE_GPIO (1 << 15)
#define GPIO_CTRL_0 0xfff80260
#define GPIO_CTRL_1 0x3 //GPIO#32,33

void init_gpio(void)
{
    printf("Initit GPIO\n");
    // Configure RGMII2 as GPIO mode
    GPIO_MODE_REG |= RGMII2_MODE_GPIO;
    // Configure GPIO as output mode
    GPIO_CTRL_0_REG |= GPIO_CTRL_0;
    GPIO_CTRL_1_REG |= GPIO_CTRL_1;
    // Pull LOW
    GPIO_DATA_0_REG &= ~(GPIO_CTRL_0);
    GPIO_DATA_1_REG &= ~(GPIO_CTRL_1);
}



