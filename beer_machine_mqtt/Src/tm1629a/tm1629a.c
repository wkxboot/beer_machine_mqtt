/*****************************************************************************
*  TM1629A驱动                                                          
*  Copyright (C) 2019 wkxboot 1131204425@qq.com.                             
*                                                                            
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file     tm1629a.c                                                   
*  @brief    TM1629A驱动                                                                                                                                                                                             
*  @author   wkxboot                                                      
*  @email    1131204425@qq.com                                              
*  @version  v1.0.0                                                  
*  @date     2019/1/11                                            
*  @license  GNU General Public License (GPL)                                
*                                                                            
*                                                                            
*****************************************************************************/

#include "tm1629a.h"


#ifndef NULL
#define NULL      ((void*)0)
#endif

#ifndef  TRUE 
#define  TRUE     (1)
#endif

#ifndef  FALSE 
#define  FALSE    (0)
#endif

#ifndef  ASSERT_NULL_PTR
#define  ASSERT_NULL_PTR(x)     \
{                               \
	     if((x)== NULL){        \
		 return -1;             \
	     }                      \
}
#endif

/*
* 命令类别
*/
#define  MODE_CMD_ID                (1<<6)
#define  DIS_CTRL_CMD_ID            (1<<7)
#define  ADDR_CMD_ID                ((1<<6)|(1<<7))

/*
*模式控制命令
*/
#define  MODE_CMD_ADDR_INCREASE     (0)
#define  MODE_CMD_ADDR_FIXED        (1<<2)
#define  MODE_CMD_NORMAL_MODE       (0)
#define  MODE_CMD_FACTORY_MODE      (1<<3)

#define  MODE_CMD_MASK              (0x0C)
/*
*地址命令掩码
*/

#define  ADDR_CMD_MASK              (0x0F)
/*
*显示控制命令
*/
#define  DIS_CTRL_CMD_ON            (1<<3)       
#define  DIS_CTRL_CMD_OFF           (0)  
#define  DIS_CTRL_DUTY_1_16         (0)
#define  DIS_CTRL_DUTY_2_16         (1)
#define  DIS_CTRL_DUTY_4_16         (2)
#define  DIS_CTRL_DUTY_10_16        (3)
#define  DIS_CTRL_DUTY_11_16        (4)
#define  DIS_CTRL_DUTY_12_16        (5)
#define  DIS_CTRL_DUTY_13_16        (6)
#define  DIS_CTRL_DUTY_14_16        (7)

#define  DIS_CTRL_CMD_MASK          (0x0F)   


static tm1629a_hal_driver_t* driver;

/*显示缓存16个字节*/
#define  BUFFER_SIZE                16
static   uint8_t buffer[BUFFER_SIZE];



static volatile uint16_t clk_delay=0;

static void delay_ns(uint16_t ns)
{
#if  (TM1629A_IF_TYPE == TM1629A_IF_TYPE_SPI)  
  return;
  
#elif (TM1629A_IF_TYPE == TM1629A_IF_TYPE_IO)
  
  clk_delay=ns;
  while(clk_delay--);
#endif
}





int tm1629a_register_hal_driver(tm1629a_hal_driver_t *hal_driver)
{
	ASSERT_NULL_PTR(hal_driver);
    
#if  (TM1629A_IF_TYPE == TM1629A_IF_TYPE_SPI)
    ASSERT_NULL_PTR(hal_driver->write_byte);
    ASSERT_NULL_PTR(hal_driver->read_byte);
    
#elif (TM1629A_IF_TYPE == TM1629A_IF_TYPE_IO)
	ASSERT_NULL_PTR(hal_driver->clk_rise);
	ASSERT_NULL_PTR(hal_driver->clk_down);
	ASSERT_NULL_PTR(hal_driver->data_set);
	ASSERT_NULL_PTR(hal_driver->data_clr);
#endif
    ASSERT_NULL_PTR(hal_driver->stb_set);
    ASSERT_NULL_PTR(hal_driver->stb_clr);
    driver=hal_driver;
	driver->registered = 1;
    return 0;
}


static void write_byte(uint8_t byte)
{
#if  (TM1629A_IF_TYPE == TM1629A_IF_TYPE_SPI)
  driver->write_byte(byte);
#elif (TM1629A_IF_TYPE == TM1629A_IF_TYPE_IO)
 uint8_t pos;
 for(pos =0; pos < 8;pos++){
  driver->clk_down();
  delay_ns(CLK_DELAY_TIME);
  if(byte & (1<<pos)){
  driver->data_set();
  }else{
  driver->data_clr();
  }
  delay_ns(CLK_DELAY_TIME);
  driver->clk_rise();
  delay_ns(CLK_DELAY_TIME);
}
#endif
}

static void start(void)
{
 driver->stb_clr();
 delay_ns(CLK_DELAY_TIME);
}

static void end(void)
{
 driver->stb_set();
 delay_ns(CLK_DELAY_TIME);
}


static void set_addr(uint8_t addr)
{
 write_byte(ADDR_CMD_ID|(ADDR_CMD_MASK & addr));
}

static void set_mode(uint8_t mode)
{
start();
write_byte(MODE_CMD_ID|(MODE_CMD_MASK & mode));
end();
}

static void display_ctrl(uint8_t dis_ctrl)
{
start();
write_byte(DIS_CTRL_CMD_ID|(DIS_CTRL_CMD_MASK & dis_ctrl));
end();
}

static void display_data(uint8_t dis_data)
{
write_byte(dis_data);
}


int tm1629a_display_refresh(void)
{
uint8_t i;

start();
set_addr(0);

for(i=0;i<BUFFER_SIZE;i++){
display_data(buffer[i]);
}

end();

return 0;
}

/*
*本地显存清零
*/

int tm1629a_buffer_clean(void)
{
uint8_t i;
for(i=0;i<BUFFER_SIZE;i++){
	buffer[i]=0;
}
return 0;
}
/*
*addr:0-15
*cnt :0-16
*/
int tm1629a_buffer_update(uint8_t addr,uint8_t update,uint8_t bits_flag)
{
if(addr >= BUFFER_SIZE){
return -1;
}
/*共阴极接法*/
#if (TM1629A_CONNECT_TYPE == TM1629A_CONNECT_TYPE_CATHODE)
uint8_t bit_pos;
for(bit_pos=0;bit_pos<8;bit_pos++){
  if(bits_flag & (1<<bit_pos)){
    if(update & (1<<bit_pos)){
    buffer[addr]|=1<<bit_pos;
    }else{
    buffer[addr]&=~(1<<bit_pos);
    }
  }
}
/*共阳极接法*/
#elif (TM1629A_CONNECT_TYPE == TM1629A_CONNECT_TYPE_ANODE)
uint8_t bit_pos;
uint8_t bit_update;
uint8_t buffer_addr;

bit_update=addr & 0x07;
buffer_addr=addr > 7 ? 1:0;

for(bit_pos=0;bit_pos<8;bit_pos++){
 if(bits_flag & (1<<bit_pos)){
 if(update &(1<<bit_pos)){
 buffer[buffer_addr+bit_pos*2] |=(1<<bit_update);
 }else{
 buffer[buffer_addr+bit_pos*2] &=~(1<<bit_update);
 }
}
}
#endif

return 0;
}

/*brightness 0-8*/
int tm1629a_brightness(uint8_t brightness )
{
if(brightness == 0){
display_ctrl(DIS_CTRL_CMD_OFF);	
}else{
display_ctrl(DIS_CTRL_CMD_ON|(brightness-1));
}
return 0;
}

int tm1629a_init()
{
 if(driver->registered){
 set_mode(MODE_CMD_ADDR_INCREASE|MODE_CMD_NORMAL_MODE);
 display_ctrl(DIS_CTRL_CMD_ON|DIS_CTRL_DUTY_14_16);
 }else{
 return -1;
 }
 
return 0;
}







