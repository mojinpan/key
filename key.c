/** ****************************************************************************
* @file 		key.c
* @author 	mojinpan
* @copyright (c) 2018 - 2020 mojinpan
* @brief 		按键驱动
* 
* @version 	V0.1 
* @date 		2011-3-2 
* @details
* 1.支持矩阵键盘和IO按键两种
* 2.支持长按键和短按键两种触发方式。
* 
* @version 	V0.2 
* @date 		2011-3-5 
* @details
* 1.最大按键数量16个,可继续扩充至64个.
* 2.支持长按键和短按键并行触发.
* 3.增加短按键部分改为按键释放时返回.
* 
* @version 	V0.3 
* @date 		2013-11-4 
* @details
* 1.重写按键获取的逻辑,长按键算法不成熟,暂时去掉
* 2.矩阵按键没用到,暂时去掉
* 3.增加按键消抖算法
* 
* @version 	V0.4 
* @date 		2013-11-6 
* @details
* 1.增加环形buf
* 2.重写接口函数,便于在OS环境中调用,当然裸奔也支持.
* c_cpp_properties.json
* @version 	V0.5 
* @date 		2013-11-6 
* @details
* 1.增加长按键的支持
* 2.增加shift键的支持
* 3.增加按键数量的定义,定义后根据按键数量选择合适的数据类型,从而节省ram和rom
* 4.处于兼容性的考虑,将最大按键支持数量由64个缩减至32个
* 
* @version 	V0.6 
* @date 		2013-11-24 
* @details
* 1.修正长按键输出后,还输出短按键的bug
* 2.修改的KeyScan()的逻辑,将长按键细分为长按键连续输出和长按键反码输出(仅一次)两种方式
* 3.shift键按下时也提供按键值输出,方便应用程序判断shift键的状态
* 4.由于第3点的修改,短按键和shift键不在冲突,即长按键/短按键/shift键相互独立
* 5.将无按键返回0xff,改成返回0
* 6.增加一个KeyFlush()函数,用于清空buf
* 
* @version 	V0.7 
* @date 		2019-04-26 
* @details
* 1.参照liunx上kfifo的代码优化环形buf的算法.
* 2.将KeyHit()函数重命名为KeyBufLen()函数,将返回有无按键改为返回按键缓冲数量.
* 3.KEY_TYPE 改为使用stdint.h中的数据类型定义,并重新启用64个按键的支持
*******************************************************************************/

#include "bsp.h"
#include "key.h"

/*******************************************************************************
                           Macro Definition    宏定义
*******************************************************************************/
#if   KEY_MAX_NUM > 32
#define KEY_TYPE    uint64_t
#elif   KEY_MAX_NUM > 16
#define KEY_TYPE    uint32_t
#elif KEY_MAX_NUM > 8
#define KEY_TYPE    uint16_t
#else
#define KEY_TYPE    uint8_t
#endif
/*******************************************************************************
                            Global Variables    全局变量
*******************************************************************************/
static  KEY_TYPE PreScanKey  = 0;               //前次按键扫描值      
static  KEY_TYPE PreReadKey  = 0;               //前次按键读取值        
static  KEY_TYPE KeyShift    = 0;               //shift按键记录
static  KEY_TYPE KeyMask     = 0;               //按键掩码
#if LONG_KEY_EN > 0
static  uint8_t  KeyPressTmr = 0;   			//长按键判断周期
#endif

static KEY_TYPE KeyBuf[KEY_BUF_SIZE];           //环形buf
static uint8_t  KeyBufInIdx  = 0;               //buf入指针                   
static uint8_t  KeyBufOutIdx = 0;               //buf出指针
/** ****************************************************************************
@brief  按键扫描函数,需根据实际硬件情况进行移植
@note   所有按键均必须高电平有效
*******************************************************************************/
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

/** ****************************************************************************
@brief  从buf中获得一个按键值
@return 返回按键值，无按键返回0xFF
*******************************************************************************/
static KEY_TYPE KeyBufOut (void)
{
    if(KeyBufOutIdx == KeyBufInIdx) return 0;            //buf空
    return KeyBuf[KeyBufOutIdx++ & (KEY_BUF_SIZE- 1)];   //返回按键值
}
/** ****************************************************************************
@brief  将按键值放入buf中
@param  code: 需放入buf的按键值
*******************************************************************************/
static void  KeyBufIn (KEY_TYPE code)
{                        
    if(KeyBufInIdx - KeyBufOutIdx == KEY_BUF_SIZE) 
    {
        KeyBufOutIdx++;//buf满则放弃最早的一个按键值
    }

    KeyBuf[KeyBufInIdx++ & (KEY_BUF_SIZE- 1)] = code ;
}

/** ****************************************************************************
@brief  按键扫描
@details
1.功能说明
  a.按键消抖
  b.捕捉长按键和短按键
  c.根据设置将按键分成短按键/长按连续建/短按shift键/长按shift键4种方式写入buf
2.算法说明
  a.按键获取算法
    1).NowKey & PreKey                                : 电平触发 
    2).NowKey ^ PreKey                                : 边缘触发 
    3).NowKey & (NowKey ^ PreKey)或(~PreKey) & NowKey : 上升沿触发
    4).PreKey & (NowKey ^ PreKey)或PreKey & (~NowKey) : 下降沿触发
  b.滤波算法
    1).PreScanKey & NowScanKey                        : 电平触发
    2).PreReadKey & (PreScanKey ^ NowScanKey)         : 采样保持
    3).NowReadKey = 1) | 2)                           : 带采样保持的电平触发
3.调用说明
  a.对下调用的KeyIOread()中,有效按键必须为高电平,且每个bit表示一个按键值
  b.应用调用该函数的间隔应该在20ms~50ms，在调用间隔内的毛刺均可滤除。
*******************************************************************************/
void KeyScan(void) 
{
  KEY_TYPE NowScanKey   = 0;                                //当前按键值扫描值
  KEY_TYPE NowReadKey   = 0;                                //当前按键值
//KEY_TYPE KeyPressDown = 0;                                //按键按下                                    
  KEY_TYPE KeyRelease   = 0;                                //按键释放                                                           
  NowScanKey  = KeyIOread();
  NowReadKey  = (PreScanKey&NowScanKey)|                    //电平触发
                 PreReadKey&(PreScanKey^NowScanKey);        //采样保持(即消抖) 

//KeyPressDown  = NowReadKey & (NowReadKey ^ PreReadKey);   //上升沿触发  
  KeyRelease    = PreReadKey & (NowReadKey ^ PreReadKey);   //下降沿触发   

#if LONG_KEY_EN > 0                                         
  if(NowReadKey == PreReadKey && NowReadKey) 				//用电平触发做长按键的有效判据
  {              
    KeyPressTmr++;
    if(KeyPressTmr >= KEY_PRESS_TMR)						//长按判断周期到,保存相应长按键值
	{                       
      if(NowReadKey & ~(KEY_LONG_SHIFT))					//长按键模式一
	  {                   
        KeyBufIn(NowReadKey | KeyShift);                    //长按键重复输出
      }
      else if(NowReadKey & (KEY_LONG_SHIFT) & ~KeyMask )	//长按键模式二 
	  {          
        KeyBufIn(~(NowReadKey | KeyShift));                 //长按键反码输出作为第二功能键      
      }
      KeyPressTmr = 0;                          			//重置连按周期,准备获取下1个长按键
      KeyMask = NowReadKey;
    }
  }
  else{
    KeyPressTmr = KEY_PRESS_DLY;                            //按键变化,重置按键判断周期
  }
#endif

  if(KeyRelease)
  {                                           				//短按键判断
      if(KeyRelease &(~KeyMask)&& !NowReadKey)
	  {
        KeyShift ^= (KeyRelease & (KEY_SHORT_SHIFT));       //shift按键码(边缘触发)
        KeyBufIn(KeyRelease | KeyShift | PreReadKey);
      }
	  else
	  {
        KeyMask = 0;
      }
  }
  
  PreScanKey = NowScanKey;
  PreReadKey = NowReadKey; 
}

/** ****************************************************************************
@brief  按键初始化
*******************************************************************************/
__weak void KeyInit( void )
{
	//按键初始化,输入，浮空，低速
	GpioInit2Input(K_UP  ,GPIO_NOPULL,GPIO_SPEED_FREQ_LOW);	
	GpioInit2Input(K_DN  ,GPIO_NOPULL,GPIO_SPEED_FREQ_LOW);		
	GpioInit2Input(K_LT  ,GPIO_NOPULL,GPIO_SPEED_FREQ_LOW);	
	GpioInit2Input(K_RT  ,GPIO_NOPULL,GPIO_SPEED_FREQ_LOW);	
	GpioInit2Input(K_ENT ,GPIO_NOPULL,GPIO_SPEED_FREQ_LOW);	
	GpioInit2Input(K_ESC ,GPIO_NOPULL,GPIO_SPEED_FREQ_LOW);	
	GpioInit2Input(K_INC ,GPIO_NOPULL,GPIO_SPEED_FREQ_LOW);	
	GpioInit2Input(K_DEC ,GPIO_NOPULL,GPIO_SPEED_FREQ_LOW);	
	GpioInit2Input(K_FUN ,GPIO_NOPULL,GPIO_SPEED_FREQ_LOW);	    
}

/** ****************************************************************************
@brief  从buf中获得一个按键值
@return 回按键值，无按键返回0xFF
*******************************************************************************/
uint32_t  KeyGet(void)
{                         
  return (uint32_t)KeyBufOut();
}     

/** ****************************************************************************
@brief  判断是否有按键按下
@return buf中按键的数量
*******************************************************************************/
uint8_t  KeyGetBufLen (void)
{
   return KeyBufInIdx - KeyBufOutIdx;
}

/** ****************************************************************************
@brief  清空buff中所有按键值
*******************************************************************************/
void KeyFlush(void)
{
   KeyBufOutIdx = KeyBufInIdx;
}
