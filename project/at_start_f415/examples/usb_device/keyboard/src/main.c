/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
  **************************************************************************
  *                       Copyright notice & Disclaimer
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

#include "at32f415_board.h"
#include "at32f415_clock.h"
#include "usb_conf.h"
#include "usb_core.h"
#include "usbd_int.h"
#include "keyboard_class.h"
#include "keyboard_desc.h"


/** @addtogroup AT32F415_periph_examples
  * @{
  */

/** @addtogroup 415_USB_device_keyboard USB_device_keyboard
  * @{
  */

/* usb global struct define */
otg_core_type otg_core_struct;
extern __IO uint8_t hid_suspend_flag;
void usb_clock48m_select(usb_clk48_s clk_s);
void keyboard_send_string(void *udev, uint8_t *string, uint8_t len);
void usb_gpio_config(void);
void usb_low_power_wakeup_config(void);
void system_clock_recover(void);
void button_exint_init(void);

/**
  * @brief  keyboard send string
  * @param  string: send string
  * @param  len: send string length
  * @retval none
  */
void keyboard_send_string(void *udev, uint8_t *string, uint8_t len)
{
  uint8_t index = 0;
  usbd_core_type *pudev = (usbd_core_type *)udev;
  keyboard_type *pkeyboard = (keyboard_type *)pudev->class_handler->pdata;
  for(index = 0; index < len; index ++)
  {
    while(1)
    {
      if(pkeyboard->g_u8tx_completed == 1)
      {
        pkeyboard->g_u8tx_completed = 0;
        usb_hid_keyboard_send_char(udev, string[index]);
        break;
      }
    }
    /* send 0x00 */
    while(1)
    {
      if(pkeyboard->g_u8tx_completed == 1)
      {
        pkeyboard->g_u8tx_completed = 0;
        usb_hid_keyboard_send_char(udev, 0x00);
        break;
      }
    }
  }

}

/**
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  __IO uint32_t delay_index = 0;
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

  system_clock_config();

  at32_board_init();

  /* usb gpio config */
  usb_gpio_config();

#ifdef USB_LOW_POWER_WAKUP
  /* enable pwc and bpr clock */
  crm_periph_clock_enable(CRM_PWC_PERIPH_CLOCK, TRUE);
  button_exint_init();
  usb_low_power_wakeup_config();
#endif

  /* enable otgfs clock */
  crm_periph_clock_enable(OTG_CLOCK, TRUE);

  /* select usb 48m clcok source */
  usb_clock48m_select(USB_CLK_HEXT);

  /* enable otgfs irq */
  nvic_irq_enable(OTG_IRQ, 0, 0);

  /* init usb */
  usbd_init(&otg_core_struct,
            USB_FULL_SPEED_CORE_ID,
            USB_ID,
            &keyboard_class_handler,
            &keyboard_desc_handler);
  at32_led_on(LED2);
  at32_led_on(LED3);
  at32_led_on(LED4);
  while(1)
  {
    if(at32_button_press() == USER_BUTTON)
    {
      if(usbd_connect_state_get(&otg_core_struct.dev) == USB_CONN_STATE_CONFIGURED)
      {
        keyboard_send_string(&otg_core_struct.dev, (uint8_t *)" Keyboard Demo\r\n", 16);
      }
      /* remote wakeup */
      if(usbd_connect_state_get(&otg_core_struct.dev) == USB_CONN_STATE_SUSPENDED
        && (otg_core_struct.dev.remote_wakup == 1))
      {
        usbd_remote_wakeup(&otg_core_struct.dev);
      }
    }

#ifdef USB_LOW_POWER_WAKUP
     /* enter deep sleep */
    if(((keyboard_type *)(otg_core_struct.dev.class_handler->pdata))->hid_suspend_flag == 1)
    {
      __disable_irq();
      if(OTG_PCGCCTL(otg_core_struct.usb_reg)->pcgcctl_bit.suspendm == 1
         && usb_suspend_status_get(otg_core_struct.usb_reg) == 1)
      {
        at32_led_off(LED2);
        at32_led_off(LED3);
        at32_led_off(LED4);
        /* congfig the voltage regulator mode */
        pwc_voltage_regulate_set(PWC_REGULATOR_LOW_POWER);

        /* enter deep sleep mode */
        pwc_deep_sleep_mode_enter(PWC_DEEP_SLEEP_ENTER_WFI);
        
        /* wait 3 LICK(maximum 120us) cycles to ensure clock stable */
        /* when wakeup from deepsleep,system clock source changes to HICK */
        if((CRM->misc2_bit.hick_to_sclk == TRUE) && (CRM->misc1_bit.hickdiv == TRUE))
        {
          /* HICK is 48MHz */
          for(delay_index = 0; delay_index < 750; delay_index++)
          {
            __NOP();
          }
        }
        else
        {
          /* HICK is 8MHz */
          for(delay_index = 0; delay_index < 125; delay_index++)
          {
            __NOP();
          }
        }
        
        system_clock_recover();
      }
      ((keyboard_type *)(otg_core_struct.dev.class_handler->pdata))->hid_suspend_flag = 0;
      __enable_irq();
      
      at32_led_on(LED2);
      at32_led_on(LED3);
      at32_led_on(LED4);
    }
 #endif

  }
}

/**
  * @brief  usb 48M clock select
  * @param  clk_s:USB_CLK_HICK, USB_CLK_HEXT
  * @retval none
  */
void usb_clock48m_select(usb_clk48_s clk_s)
{
  crm_clocks_freq_type clocks_struct;
  
  crm_clocks_freq_get(&clocks_struct);
  switch(clocks_struct.sclk_freq)
  {
    /* 48MHz */
    case 48000000:
      crm_usb_clock_div_set(CRM_USB_DIV_1);
      break;

    /* 72MHz */
    case 72000000:
      crm_usb_clock_div_set(CRM_USB_DIV_1_5);
      break;

    /* 96MHz */
    case 96000000:
      crm_usb_clock_div_set(CRM_USB_DIV_2);
      break;

    /* 120MHz */
    case 120000000:
      crm_usb_clock_div_set(CRM_USB_DIV_2_5);
      break;

    /* 144MHz */
    case 144000000:
      crm_usb_clock_div_set(CRM_USB_DIV_3);
      break;

    default:
      break;
  }
}

/**
  * @brief  this function config gpio.
  * @param  none
  * @retval none
  */
void usb_gpio_config(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(OTG_PIN_GPIO_CLOCK, TRUE);
  gpio_default_para_init(&gpio_init_struct);

  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;

#ifdef USB_SOF_OUTPUT_ENABLE
  crm_periph_clock_enable(OTG_PIN_SOF_GPIO_CLOCK, TRUE);
  gpio_init_struct.gpio_pins = OTG_PIN_SOF;
  gpio_init(OTG_PIN_SOF_GPIO, &gpio_init_struct);
#endif

  /* otgfs use vbus pin */
#ifndef USB_VBUS_IGNORE
  gpio_init_struct.gpio_pins = OTG_PIN_VBUS;
  gpio_init_struct.gpio_pull = GPIO_PULL_DOWN;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init(OTG_PIN_GPIO, &gpio_init_struct);
#endif


}
#ifdef USB_LOW_POWER_WAKUP
/**
  * @brief  usb low power wakeup interrupt config
  * @param  none
  * @retval none
  */
void usb_low_power_wakeup_config(void)
{
  exint_init_type exint_init_struct;

  exint_default_para_init(&exint_init_struct);

  exint_init_struct.line_enable = TRUE;
  exint_init_struct.line_mode = EXINT_LINE_INTERRUPT;
  exint_init_struct.line_select = OTG_WKUP_EXINT_LINE;
  exint_init_struct.line_polarity = EXINT_TRIGGER_RISING_EDGE;
  exint_init(&exint_init_struct);

  nvic_irq_enable(OTG_WKUP_IRQ, 0, 0);
}

/**
  * @brief  system clock recover.
  * @param  none
  * @retval none
  */
void system_clock_recover(void)
{
  /* enable external high-speed crystal oscillator - hext */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_HEXT, TRUE);

  /* wait till hext is ready */
  while(crm_hext_stable_wait() == ERROR);

  /* enable pll */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);

  /* wait till pll is ready */
  while(crm_flag_get(CRM_PLL_STABLE_FLAG) == RESET);

  /* enable auto step mode */
  crm_auto_step_mode_enable(TRUE);

  /* select pll as system clock source */
  crm_sysclk_switch(CRM_SCLK_PLL);

  /* wait till pll is used as system clock source */
  while(crm_sysclk_switch_status_get() != CRM_SCLK_PLL);
}

/**
  * @brief  configure button exint
  * @param  none
  * @retval none
  */
void button_exint_init(void)
{
  exint_init_type exint_init_struct;

  crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
  gpio_exint_line_config(GPIO_PORT_SOURCE_GPIOA, GPIO_PINS_SOURCE0);

  exint_default_para_init(&exint_init_struct);
  exint_init_struct.line_enable = TRUE;
  exint_init_struct.line_mode = EXINT_LINE_INTERRUPT;
  exint_init_struct.line_select = EXINT_LINE_0;
  exint_init_struct.line_polarity = EXINT_TRIGGER_RISING_EDGE;
  exint_init(&exint_init_struct);

  nvic_irq_enable(EXINT0_IRQn, 0, 0);
}

/**
  * @brief  this function handles otgfs wakup interrupt.
  * @param  none
  * @retval none
  */
void OTG_WKUP_HANDLER(void)
{
  exint_flag_clear(OTG_WKUP_EXINT_LINE);
}

/**
  * @brief  exint0 interrupt handler
  * @param  none
  * @retval none
  */
void EXINT0_IRQHandler(void)
{
  exint_flag_clear(EXINT_LINE_0);
}


#endif

/**
  * @brief  this function handles otgfs interrupt.
  * @param  none
  * @retval none
  */
void OTG_IRQ_HANDLER(void)
{
  usbd_irq_handler(&otg_core_struct);
}

/**
  * @brief  usb delay millisecond function.
  * @param  ms: number of millisecond delay
  * @retval none
  */
void usb_delay_ms(uint32_t ms)
{
  /* user can define self delay function */
  delay_ms(ms);
}

/**
  * @brief  usb delay microsecond function.
  * @param  us: number of microsecond delay
  * @retval none
  */
void usb_delay_us(uint32_t us)
{
  delay_us(us);
}

/**
  * @}
  */

/**
  * @}
  */
