/** ****************************************************************************
* @file 	key.h
* @author 	mojinpan
* @copyright (c) 2018 - 2020 mojinpan
* @brief 	按键驱动
* 
* @par 功能说明
* @details
* 1.支持最大64个按键(数量可配置)
* 2.支持按键消抖处理(可滤掉小于2个扫描周期的毛刺)
* 3.支持按键环形缓冲区(长度可配置)
* 4.支持4种不同按键模式:A:短按键 B:shift键 C:长按单发 D:长按连发
* 5.短按键    :短按直接输出键值
* 6.shift键  :点击后除了输出一次短按键外,还将后续所有其他按键的键值改为,shift键+按键值(包括长短按键)
* 7.长按连发  :长按一定时间后连续输出按键键值
* 8.长按单发  :长按一定时间后反码输出键值(仅一次)
* 9.4种不同按键模式可相互叠加使用,所有按键功能都能并发输出
* 
* @par 移植说明 
* @details
* 1.根据具体情况编写KeyIOread()和KeyInit().
* 2.每20ms~50ms调用KeyScan()扫描按键,调用KeyGetBufLen()判断是否有按键,调用KeyGet()获得按键值.
*******************************************************************************/
#ifndef __KEY__
#define __KEY__

/*******************************************************************************
                              Header Files        头文件
*******************************************************************************/
#include "bsp.h"
#include "stdbool.h"
#include "stdint.h"
/*******************************************************************************
                               Key Scancode        按键扫描码
*******************************************************************************/
///* @brief 按键键值,每个按键占1bit,不得重复
typedef enum
{
    KEY_UP    	 = 0x0001,
    KEY_DN    	 = 0x0002,     
    KEY_LT    	 = 0x0004,
    KEY_RT    	 = 0x0008,  
    KEY_ENT   	 = 0x0010, 
    KEY_ESC   	 = 0x0020,
    KEY_INC   	 = 0x0040,
    KEY_DEC   	 = 0x0080,
    KEY_FUN   	 = 0x0100
	
    KEY_UP_L 	 = 0xFFFE,	
    KEY_DN_L 	 = 0xFFFD,
	
    KEY_UP_SF    = 0x0101,
    KEY_DN_SF    = 0x0102,
    KEY_LT_SF    = 0x0104,
    KEY_RT_SF    = 0x0108,  
    KEY_ENT_SF   = 0x0110,
    KEY_ESC_SF   = 0x0120,
    KEY_INC_SF   = 0x0140,
    KEY_DEC_SF   = 0x0180	
} KEY_value;
/*******************************************************************************
                                Drive Config        驱动配置
*******************************************************************************/
#define KEY_MAX_NUM       11      		 //定义最大按键数量(1~64)
#define KEY_BUF_SIZE      8       		 //定义按键缓冲区大小(2,4,8,16,32,64,128,256)
#define KEY_LONG_EN       1       		 //定义是否支持长按键(1 or 0)
#define KEY_SHORT_SHIFT   KEY_FUN   	 //定义shift键,短按有效(0 or 按键扫描码)

#if KEY_LONG_EN > 0
#define KEY_PRESS_DLY     40      	     //定义长按键延时n个扫描周期开始判定有效值(1~256)
#define KEY_PRESS_TMR     5       		 //定义长按键n个扫描周期为1次有效扫描值(1~256)
#define KEY_LONG_SHIFT    KEY_UP|KEY_DN  //长按键反码输出(仅一次),未定义的长按连续输出(0 or 按键扫描码)
#endif
/*******************************************************************************
								Function declaration 函数声明
*******************************************************************************/
void KeyInit(void);               //按键初始化函数
void KeyScan(void);               //按键扫描函数,20ms~50ms定时调用
uint8_t KeyGetBufLen(void);       //按键判断函数,判断是否有按键按下
uint32_t KeyGet(void);            //按键获得函数,获取具体按键值,无按键时返回0
void KeyFlush(void);			  //清空按键buf
#endif



