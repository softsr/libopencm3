/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2011 Stephen Caudle <scaudle@doceme.com>
 * Copyright (C) 2012 Daniel Serpell <daniel.serpell@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/timer.h>

void clock_setup(void)
{
	/* Enable high-speed clock at 168MHz */
	rcc_clock_setup_hse_3v3(&hse_12mhz_3v3[CLOCK_3V3_168MHZ]);

	/* Enable GPIOD clock for LED & USARTs. */
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPCEN);
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPAEN);

	/* Enable clocks for USART3. */
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_USART3EN);
}

void usart_setup(void)
{
	/* Setup USART3 parameters. */
	usart_set_baudrate(USART3, 115200);
	usart_set_databits(USART3, 8);
	usart_set_stopbits(USART3, USART_STOPBITS_1);
	usart_set_mode(USART3, USART_MODE_TX);
	usart_set_parity(USART3, USART_PARITY_NONE);
	usart_set_flow_control(USART3, USART_FLOWCONTROL_NONE);

	/* Finally enable the USART. */
	usart_enable(USART3);
}

void gpio_setup(void)
{
	/* Setup GPIO pin GPIO12 on GPIO port D for LED. */
	gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);

	/* Setup GPIO pins for USART3 transmit. */
	gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO10);

	/* Setup USART3 TX pin as alternate function. */
	gpio_set_af(GPIOC, GPIO_AF7, GPIO10);
}

void timer_setup(void)
{
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_TIM2EN);
	timer_reset(TIM2);
	timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT_MUL_4,
               TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_set_period(TIM2, 0xFFFFFFFF);
	timer_set_prescaler(TIM2, 0xFFFF);
	timer_enable_counter(TIM2);
}

/* Maximum number of iterations for the escape-time calculation */
#define maxIter 32
/* This array converts the iteration count to a character representation. */
static char color[maxIter+1] = " .:++xxXXX%%%%%%################";

/* Main mandelbrot calculation */
static int iterate(float px, float py)
{
	int it=0;
	float x=0,y=0;
	while(it<maxIter)
	{
		float nx = x*x;
		float ny = y*y;
		if( (nx + ny) > 4 )
			return it;
		// Zn+1 = Zn^2 + P
		y = 2*x*y + py;
		x = nx - ny + px;
		it++;
	}
	return 0;
}

static void mandel(float cX, float cY, float scale)
{
	int x,y;
	for(x=-60;x<60;x++)
	{
		for(y=-50;y<50;y++)
		{
			int i = iterate(cX+x*scale, cY+y*scale);
			//usart_send_blocking(USART3, color[i]);
		}
		//usart_send_blocking(USART3, '\r');
		//usart_send_blocking(USART3, '\n');
	}
}

static void ascii(char byte)
{
	char hunds, tens, ones;
	hunds = byte / 100;
	usart_send_blocking(USART3, hunds + 48);
	tens = (byte - hunds*100)/10;
	usart_send_blocking(USART3, tens + 48);
	ones = byte - hunds*100 - tens*10;
	usart_send_blocking(USART3, ones + 48);
}

int main(void)
{
	float scale = 0.25f, centerX = -0.5f, centerY = 0.0f;
	long time_past = 0, time_now = 0, time = 0;
	long cnt = 0;

	clock_setup();
	gpio_setup();
	usart_setup();
	timer_setup();

	while (1) {
		/* Blink the LED (PD12) on the board with each fractal drawn. */
		if(cnt++ > 1000)
		{
			cnt = 0;
			gpio_toggle(GPIOA, GPIO13);	/* LED on/off */
		}
		mandel(centerX,centerY,scale);	/* draw mandelbrot */

		/* Change scale and center */
		centerX += 0.175f * scale;
		centerY += 0.522f * scale;
		scale	*= 0.875f;
		
		time_now = timer_get_counter(TIM2);
		if(time_now > time_past)
		{
			time = time_now - time_past;
			time_past = time_now;
			ascii((time>>24)&0xFF); usart_send_blocking(USART3, ' ');
			ascii((time>>16)&0xFF); usart_send_blocking(USART3, ' ');
			ascii((time>>8)&0xFF); usart_send_blocking(USART3, ' ');
			ascii(time&0xFF);
			usart_send_blocking(USART3, '\r');
			usart_send_blocking(USART3, '\n');
		}
		
		//usart_send_blocking(USART3, '\r');
		//usart_send_blocking(USART3, '\n');
	}

	return 0;
}
