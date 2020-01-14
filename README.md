# key: 一个精简但功能强大的按键驱动

![version](https://img.shields.io/badge/version-0.7-brightgreen.svg)
![build](https://img.shields.io/badge/build-2019.4.26-brightgreen.svg)
![build](https://img.shields.io/badge/license-MIT-brightgreen.svg)

## 1. 介绍

这是一个通过位操作实现状态机转换的按键驱动,通过独特的算法,在非常非常小的资源消耗下实现短按键/shift键/长按连发,长按单发等多种复杂的按键功能。

## 2. 原理介绍

### 2.1 键值码
- 本按键驱动键值码的方式返回按键输入状态,键值码中每一个位表示一个按键。
- 键值码根据配置长度从8位到~64位(即最多支持64个按键)。
- 键值码根据实际情况分成短按键值码shift键值码,长按键值码
```
    短按键值码  = 键值码
    shift键值码 = shift键值码 | 键值码.
    长按键值码  = ~键值码
```

### 3.算法说明

```
//PreKey:前次按键值
//NowKey:当前按键值
//PreScanKey:前次采样值 
//NowReadKey:当前采样值 
  a.按键获取算法
    1).NowKey & PreKey                                : 电平触发 
    2).NowKey ^ PreKey                                : 边缘触发 
    3).NowKey & (NowKey ^ PreKey)或(~PreKey) & NowKey : 上升沿触发
    4).PreKey & (NowKey ^ PreKey)或PreKey & (~NowKey) : 下降沿触发
  b.滤波算法
    1).PreScanKey & NowScanKey                        : 电平触发
    2).PreReadKey & (PreScanKey ^ NowScanKey)         : 采样保持
    3).NowReadKey = 1) | 2)                           : 带采样保持的电平触发
//以上算法可通过列真值表推导
```

## 4.功能说明

- 支持最大64个按键(数量可配置)。
- 支持按键消抖处理(可滤掉小于2个扫描周期的毛刺)。
- 支持按键环形缓冲区(长度可配置)。
- 支持4种不同按键模式:A:短按键 B:shift键 C:长按单发 D:长按连发。
- 短按键    :短按直接输出键值。
- shift键   :点击后除了输出一次shift键的键值码外,还将后续所有其他按键的键值改为shift键值码
- 长按连发  :长按一定时间后连续输出按键键值。
- 长按单发  :长按一定时间后反码输出键值(仅一次)
- 4种不同按键模式可相互叠加使用,所有按键功能都能并发输出。

## 5. 移植说明

- 使用宏或枚举定义按键键值码。(长按单发使用反码表示)
```
typedef enum
{
    //短按键键值码
    KEY_UP    = 0x0001,
    KEY_DN    = 0x0002,
    KEY_LT    = 0x0004,
    KEY_RT    = 0x0008,  
    KEY_ENT   = 0x0010,
    KEY_ESC   = 0x0020,
    KEY_INC   = 0x0040,
    KEY_DEC   = 0x0080,
    KEY_FUN   = 0x0100,
    //长按键键值码
    KEY_UP_L 	 = 0xFFFE,	
    KEY_DN_L 	 = 0xFFFD,
    //shift按键键值码
    KEY_UP_SF    = 0x0101,
    KEY_DN_SF    = 0x0102,
    KEY_LT_SF    = 0x0104,
    KEY_RT_SF    = 0x0108,  
    KEY_ENT_SF   = 0x0110,
    KEY_ESC_SF   = 0x0120,
    KEY_INC_SF   = 0x0140,
    KEY_DEC_SF   = 0x0180
} KEY_value;
```
- 根据具体硬件环境编写按键初始化函数KeyInit()。
- 根据编写按键读取函数KeyIOread()，注意所有按键均需定义为高电平有效。

```
__weak static KEY_TYPE  KeyIOread( void )
{
    KEY_TYPE KeyScanCode=0;
    
	KeyScanCode |= GPIO_IN(K_UP ) ? 0 : KEY_UP ;
	KeyScanCode |= GPIO_IN(K_DN ) ? 0 : KEY_DN ;
	KeyScanCode |= GPIO_IN(K_LT ) ? 0 : KEY_LT ;
	KeyScanCode |= GPIO_IN(K_RT ) ? 0 : KEY_RT ;
	KeyScanCode |= GPIO_IN(K_ENT) ? 0 : KEY_ENT;
	KeyScanCode |= GPIO_IN(K_ESC) ? 0 : KEY_ESC;
	KeyScanCode |= GPIO_IN(K_INC) ? 0 : KEY_INC;
	KeyScanCode |= GPIO_IN(K_DEC) ? 0 : KEY_DEC;
	KeyScanCode |= GPIO_IN(K_FUN) ? 0 : KEY_FUN;
	
  return KeyScanCode;
}
```

## 6. 使用说明

- 按需求配置宏

    | 宏                            | 意义                                                      |
    | ----------------------------- | -------------------------------------------------------- |
    | KEY_MAX_NUM                   | 定义最大按键数量(1~64)                                     |
    | KEY_BUF_SIZE                  | 定义按键缓冲区大小(2,4,8,16,32,64,128,256)                 |
    | KEY_LONG_EN                   | 定义是否支持长按键(1 or 0)                                 |
    | KEY_SHORT_SHIFT               | 定义shift键,短按有效(0 or 按键键值码)                       |
    | KEY_PRESS_DLY                 | 定义长按键延时n个扫描周期开始判定有效值(1~255)               |
    | KEY_PRESS_TMR                 | 定义长按键n个扫描周期为1次有效扫描值(1~255)                  |
    | KEY_LONG_SHIFT                | 长按键反码输出(仅一次),未定义的长按连续输出(0 or 按键键值码)  |
    
- 每20ms~50ms调用KeyScan()扫描按键。
- 调用KeyGetBufLen()判断是否有按键,调用KeyGet()获得按键值。

```
void KeyTest(void)
{
	while(1)
	{
		
		KeyScan();
		delay_ms(20);
		KeyScan();

			switch(KeyGet())
			{
				case  KEY_UP : printf("Key Read : UP \r\n");break;
				case  KEY_DN : printf("Key Read : DN \r\n");break;			
				case  KEY_LT : printf("Key Read : LT \r\n");break;	
				case  KEY_RT : printf("Key Read : RT \r\n");break;
				case  KEY_ENT: printf("Key Read : ENT\r\n");break;			
				case  KEY_ESC: printf("Key Read : ESC\r\n");break;	
				case  KEY_INC: printf("Key Read : INC\r\n");break;
				case  KEY_DEC: printf("Key Read : DEC\r\n");break;			
				case  KEY_FUN: printf("Key Read : FUN\r\n");break;
				case  KEY_UP_L : printf("Key Read : UP for long key \r\n");break;
				case  KEY_DN_L : printf("Key Read : DN for long key \r\n");break;	
				case  KEY_UP_SF : printf("Key Read : UP with shift key \r\n");break;
				case  KEY_DN_SF : printf("Key Read : DN with shift key \r\n");break;			
				case  KEY_LT_SF : printf("Key Read : LT with shift key \r\n");break;	
				case  KEY_RT_SF : printf("Key Read : RT with shift key \r\n");break;
				case  KEY_ENT_SF: printf("Key Read : ENT with shift key \r\n");break;			
				case  KEY_ESC_SF: printf("Key Read : ESC with shift key \r\n");break;	
				case  KEY_INC_SF: printf("Key Read : INC with shift key \r\n");break;
				case  KEY_DEC_SF: printf("Key Read : DEC with shift key \r\n");break;			
				default :	break;		
			}
	}	
	
}
```

## 8. 更新说明

- V0.1 2011-3-2
  
  - 1.支持矩阵键盘和IO按键两种
  - 2.支持长按键和短按键两种触发方式。

- V0.2 2011-3-5

  - 1.最大按键数量16个,可继续扩充至64个.
  - 2.支持长按键和短按键并行触发.
  - 3.增加短按键部分改为按键释放时返回.

- V0.3 2013-11-4 
  - 1.重写按键获取的逻辑,长按键算法不成熟,暂时去掉
  - 2.矩阵按键没用到,暂时去掉
  - 3.增加按键消抖算法

- V0.4 2013-11-6
  - 1.增加环形buf
  - 2.重写接口函数,便于在OS环境中调用,当然裸奔也支持.

- V0.5 2013-11-6 
  - 1.增加长按键的支持
  - 2.增加shift键的支持
  - 3.增加按键数量的定义,定义后根据按键数量选择合适的数据类型,从而节省ram和rom
  - 4.处于兼容性的考虑,将最大按键支持数量由64个缩减至32个

- V0.6 2013-11-24
  - 1.修正长按键输出后,还输出短按键的bug
  - 2.修改的KeyScan()的逻辑,将长按键细分为长按键连续输出和长按键反码输出(仅一次)两种方式
  - 3.shift键按下时也提供按键值输出,方便应用程序判断shift键的状态
  - 4.由于第3点的修改,短按键和shift键不在冲突,即长按键/短按键/shift键相互独立
  - 5.将无按键返回0xff,改成返回0
  - 6.增加一个KeyFlush()函数,用于清空buf

- V0.7 2019-04-26
  - 1.参照liunx上kfifo的代码优化环形buf的算法.
  - 2.将KeyHit()函数重命名为KeyBufLen()函数,将返回有无按键改为返回按键缓冲数量.
  - 3.KEY_TYPE 改为使用stdint.h中的数据类型定义,并重新启用64个按键的支持.
  
