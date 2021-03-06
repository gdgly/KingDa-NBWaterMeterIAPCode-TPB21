/*********************************************************************************
//概述：
//作者：随风飘跃     时间：       地点：
//CPU型号：         系统主频：
//版本号：V0.0        
*********************************************************************************/
/*********************************************************************************
文件包含区
*********************************************************************************/
#include "IAP.h"
/*********************************************************************************
常量定义区
*********************************************************************************/

/*********************************************************************************
公共变量定义区
*********************************************************************************/
/*********************************************************************************
外部变量声明区
*********************************************************************************/
/*********************************************************************************
私有变量定义区
*********************************************************************************/
struct Upgrade_Str Upgrade;
unsigned char APPValid;
/*********************************************************************************
测试变量定义区
*********************************************************************************/
/*********************************************************************************
内部函数定义区
*********************************************************************************/
/*********************************************************************************
功能代码定义区
*********************************************************************************/
/*********************************************************************************
 Function:      //
 Description:   //验证是否运行APP程序
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void Check_Run_APP(void)
{
  Read_APP_Valid();
  Read_Upgrade_Info();
  
  //如果APP程序有效且不用升级，则运行APP程序
  if( (APPValid == APP_VALID)&&(Upgrade.Process == IDLE) )
  {
    STM8_Interrupt_Vector_Table_Redirection();
    JumptoAPP();
  }
}

/*********************************************************************************
 Function:      //
 Description:   //运行APP程序
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void Run_APP(void)
{
  STM8_Interrupt_Vector_Table_Redirection();
  JumptoAPP();
}
/*********************************************************************************
 Function:      //
 Description:   //保存BootLoader程序中断向量表
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void STM8_Interrupt_Vector_Table_Init(void)
{
  unsigned char Index = 0;
  uint32_t data = 0;
  uint32_t Vector[32] = {0};
  
  FLASH_Unlock(FLASH_MemType_Program);    
  //如果BLD中断向量表次地址和APP中断向量表次地址相同；
  //则表明程序从APP异常复位进入BLD程序
  if(FLASH_ReadWord(0x8000+4) == (0x82000000+APP_START_ADDR+4))
  {
    for(Index = 1; Index < 0x20;Index++)
    {
      data = EEPROM_ReadWord(INTERRUPT_VECTOR_ADD+4*Index);
      FLASH_ProgramWord(0x8000+4*Index,data);
    }
  }
  else
  {
    for(Index = 1; Index < 0x20;Index++)
    {
      Vector[Index] = FLASH_ReadWord(0x8000+4*Index);
    }
    WriteRom (INTERRUPT_VECTOR_ADD,Vector,128);
  }
  FLASH_Lock(FLASH_MemType_Program);

}
/*********************************************************************************
 Function:      //
 Description:   //读EEPROM地址数据
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
uint32_t EEPROM_ReadWord(uint32_t Address)
{
 return(*(PointerAttr uint32_t *) (uint32_t)Address);       
}
/*********************************************************************************
 Function:      //
 Description:   //读FLASH地址数据
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
uint32_t FLASH_ReadWord(uint32_t Address)
{
 return(*(PointerAttr uint32_t *) (uint32_t)Address);       
}

/*********************************************************************************
 Function:      //
 Description:   ////重新初始化STM8的中断向量表  把它重新定义到APP的中断向量中
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void STM8_Interrupt_Vector_Table_Redirection(void)
{
  disableInterrupts();   //关闭中断  
  uint8_t Index = 0;	
  
  FLASH_Unlock(FLASH_MemType_Program);
  for(Index = 1; Index < 0x20;Index++)
  {
   if(FLASH_ReadWord(0x8000+4*Index)!=(0x82000000+APP_START_ADDR+Index*4))
   {
    FLASH_ProgramWord(0x8000+4*Index,0x82000000+APP_START_ADDR+Index*4);
   }
  }
  FLASH_Lock(FLASH_MemType_Program);
}
/*********************************************************************************
 Function:      //
 Description:   //将升级包写入Flash
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
ErrorStatus FlashWrite (void *pbuff, unsigned short length)
{
 unsigned short itemp;
 unsigned char *p; 
 unsigned short itemp16;
 
 do
 {FLASH_Unlock(FLASH_MemType_Program); }//开启FLASH编程
 while (FLASH_GetFlagStatus(FLASH_FLAG_PUL) == RESET); 
 itemp = 0;
 p = pbuff;
 while (itemp < length)
 {
   FLASH_ProgramByte(Upgrade.ProgramAddr ,p[itemp]);
   itemp16 = 0xffff;
   while(FLASH_GetFlagStatus(FLASH_FLAG_EOP) == RESET)
   {
    itemp16 --;
    if(itemp16 <= 1)
    {
      FLASH_Lock(FLASH_MemType_Program);
      return ERROR;
    }
   }
   itemp16 = FLASH_ReadByte(Upgrade.ProgramAddr);
   if(p[itemp] != itemp16)
   {
     FLASH_Lock(FLASH_MemType_Program);
     return ERROR;
   }
   else
   {itemp ++;Upgrade.ProgramAddr++;}
 }
 FLASH_Lock(FLASH_MemType_Program);
 return SUCCESS;
}
/*********************************************************************************
 Function:      //
 Description:   //跳转到应用程序运行
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void JumptoAPP(void)
{
  sim();               // disable interrupts，建议程序跳转前关中断，跳转到新程序后先清一次中断。
  asm("LDW X,  SP ");
  asm("LD  A,  $FF");
  asm("LD  XL, A  ");
  asm("LDW SP, X  ");
  asm("JPF $D000");
}
/*********************************************************************************
 Function:      //
 Description:   //升级接受流程
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void Upgrade_Recv_Process(unsigned char *str)
{
  unsigned short i = 0,j = 0;
  unsigned char MessageID = 0;
  unsigned short crc16 = 0;
  ErrorStatus ProgramStatus = SUCCESS;
  unsigned char APPVersion[16] = {0};
  
  memset(Upgrade.Buffer,'\0',Upgrade.Length);//清缓存
  Upgrade.Length = 0;
  
  i = 6;
  while(str[i] != ',')
  {
    if( (str[i] >= '0')&&(str[i] <= '9') )
    {
      Upgrade.Length = Upgrade.Length*10+ASCLL_to_Int(str[i]);
    }
    i++;
  }
 
  for(j = 0;j < Upgrade.Length;j++)
  {
    Upgrade.Buffer[j] = ASCLL_to_Int(str[i+1+2*j])*16+ASCLL_to_Int(str[i+2+2*j]);
  }

  crc16 = Upgrade.Buffer[4]*256+Upgrade.Buffer[5];
  Upgrade.Buffer[4] = 0;
  Upgrade.Buffer[5] = 0;
  
  //CRC16校验
  if(crc16 != CRC16((unsigned char*)Upgrade.Buffer,Upgrade.Length))
  {
    return;
  }    
   
  MessageID = Upgrade.Buffer[3];
  
  switch(MessageID)
  {
    case MESSAGE19:            //查询设备版本
      {
        //+NNMI:8,FFFE01134C9A0000
        Upgrade.Process = MESSAGE19;
        Upgrade.Incident_Pend = TRUE; 
        Upgrade.TimeoutCounter = UPGRADE_TIMEOUT_MAX;
        Delete_Timer(Upgrade_TimeOut_CallBack);//删除升级超时回调    
      }
      break;
    case MESSAGE20:           //新版本通知
      {
        //+NNMI:30,FFFE01145A5A0016 4E425F4150505F56312E310000000000 01F4 0032 0000 
        Upgrade.Process = MESSAGE20;
        Read_Version(APP_VERSION_ADD,APPVersion);
        if( 0 == memcmp(APPVersion,&Upgrade.Buffer[8],16) )     //已经是最新版本
        {
          Upgrade.ResultCode = 0x03;
        }
        else                                                    //可以升级
        {
          Upgrade.ResultCode = 0x00;
          memcpy(Upgrade.Version,&Upgrade.Buffer[8],16);
          Upgrade.PackageSize = Upgrade.Buffer[24]*256+Upgrade.Buffer[25];
          Upgrade.PackageTotalNum = Upgrade.Buffer[26]*256+Upgrade.Buffer[27]; 
        }
        Upgrade.Incident_Pend = TRUE;
        Upgrade.TimeoutCounter = UPGRADE_TIMEOUT_MAX;
        Delete_Timer(Upgrade_TimeOut_CallBack);//删除升级超时回调
      } 
      break;
    case MESSAGE21:            //下发升级包
      {
        Upgrade.Process = MESSAGE21;
        if(Upgrade.Buffer[8] == 0x00)     //平台处理成功
        {
          //写入升级包之前将APP有效标志清零
          if( APPValid == APP_VALID)
          {
            APPValid = 0;
            Save_APP_Valid();
          }
               
          Upgrade.PackageNum = Upgrade.Buffer[9]*256+Upgrade.Buffer[10];
          Upgrade.ProgramAddr = APP_START_ADDR+(Upgrade.PackageNum*Upgrade.PackageSize);
         
          ProgramStatus = FlashWrite(&Upgrade.Buffer[11],Upgrade.PackageSize);
          if(ProgramStatus != ERROR)
          {
            if(Upgrade.PackageNum == (Upgrade.PackageTotalNum-1))
            {
              Upgrade.Process = MESSAGE22;
              Upgrade.Incident_Pend = TRUE;
              Upgrade.TimeoutCounter = UPGRADE_TIMEOUT_MAX;
              Delete_Timer(Upgrade_TimeOut_CallBack);//删除升级超时回调
              
            }
            else
            {
              Upgrade.PackageNum++;
              Upgrade.Incident_Pend = TRUE;
              Upgrade.TimeoutCounter = UPGRADE_TIMEOUT_MAX;
              Delete_Timer(Upgrade_TimeOut_CallBack);//删除升级超时回调       
            }
          }
          else
          {
            Upgrade.Incident_Pend = TRUE;
            Delete_Timer(Upgrade_TimeOut_CallBack);//删除升级超时回调  
          }
        }
      }
      break;
    case MESSAGE22:            //设备上报升级包下载状态，平台响应
      {
        Upgrade.Process = MESSAGE22;
      }
      break;
    case MESSAGE23:            //平台命令升级
      {
        Upgrade.Process = MESSAGE23;
        Upgrade.Incident_Pend = TRUE;
        Upgrade.TimeoutCounter = UPGRADE_TIMEOUT_MAX;
        Delete_Timer(Upgrade_TimeOut_CallBack);//删除升级超时回调
      }
      break;
    default:
      break;
  }
}
/*********************************************************************************
 Function:      //
 Description:   //升级发送进程
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void Upgrade_Send_Process(void)
{
  
  switch(Upgrade.Process)
  {
    case MESSAGE19:            //平台查询设备版本，设备响应
      { 
        SendUpgradeMessage19();
        Create_Timer(ONCE,UPGRADE_TIMEROUT_TIME,
                     Upgrade_TimeOut_CallBack,0,PROCESS);//升级超时回调
      }
      break;
    case MESSAGE20:           //平台通知新版本，设备响应
      {         
        if(Upgrade.ResultCode == 0x03)  //已经是最新版本
        {
          Upgrade.Process = IDLE;
          Save_Upgrade_Info();
        
          Upgrade.Incident_Pend = FALSE;
          Create_Timer(ONCE,2,
                       MCU_DeInit,0,PROCESS);//延时2s
        }
        else if(Upgrade.ResultCode == 0x00)
        {
          Upgrade.PackageNum = 0;
          Upgrade.Process = MESSAGE21;
          Upgrade.TimeoutCounter = UPGRADE_TIMEOUT_MAX; 
          Create_Timer(ONCE,2,
                       Upgrade_Delay_CallBack,0,PROCESS);//延时2s请求第一包数据
        }  
        
        SendUpgradeMessage20(); 
      } 
      break;
    case MESSAGE21:            //请求升级包
      {
        SendUpgradeMessage21();//请求下一包升级包
        Create_Timer(ONCE,UPGRADE_TIMEROUT_TIME,
                     Upgrade_TimeOut_CallBack,0,PROCESS);//升级超时回调
      }
      break;
    case MESSAGE22:            //设备上报升级包下载状态
      {
        SendUpgradeMessage22();        //上报升级包下载完成  
        Create_Timer(ONCE,UPGRADE_TIMEROUT_TIME,
                     Upgrade_TimeOut_CallBack,0,PROCESS);//升级超时回调
      }
      break;
    case MESSAGE23:            //平台命令升级，设备响应
      {
        //保存当前水量
        Save_Add_Flow(ADD_FLOW_ADD,&Cal.Water_Data);
        //保存升级信息
        Upgrade.Process = MESSAGE24;
        Save_Upgrade_Info();
        //保存新版本
        Save_Version(APP_VERSION_ADD,Upgrade.Version);
        //跳转到应用程序
        Create_Timer(ONCE,5,
                     Run_APP,0,PROCESS);
        
        SendUpgradeMessage23();
      }
      break;
    case MESSAGE24:            //设备上报升级成功
      {
      }
      break;
    default:
      break;
  }
}

/*********************************************************************************
 Function:      //
 Description:   //平台查询软件版本，设备响应报文
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void SendUpgradeMessage19(void)
{
  unsigned char i = 0;
  unsigned short crc16 = 0;
  unsigned char version[16] = "NB_APP_V2.0";
  unsigned char buff[26] = {0};
  uint8_t data[70] = "AT+MLWULDATA=25,00000000000000000000000000000000000000000000000000\r\n";
  
  //如果APPValid有效，取eeprom数据，否则取默认值
  if(APPValid == APP_VALID)
  {
    Read_Version(APP_VERSION_ADD,version);
  }
  
  buff[0] = 0xFF;
  buff[1] = 0xFE;
  buff[2] = 0x01;
  buff[3] = MESSAGE19;
  buff[4] = 0;
  buff[5] = 0;
  buff[6] = 0;
  buff[7] = 17;
  buff[8] = 0;
  memcpy(&buff[9],version,16);
  
  crc16 = CRC16(buff,25);
  buff[4] = (crc16>>8)&0xFF;
  buff[5] = crc16&0xFF;
  
  for(i = 0;i < 25;i++)    
  {
    data[16+2*i] = Int_to_ASCLL(buff[i]/0x10);
    data[17+2*i] = Int_to_ASCLL(buff[i]%0x10);
    
  }

  Uart2_Send(data,68);
}
/*********************************************************************************
 Function:      //
 Description:   //平台通知软件新版本，设备响应报文
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void SendUpgradeMessage20(void)
{
  unsigned char i = 0;
  unsigned short crc16 = 0;
  unsigned char buff[10] = {0};
  uint8_t data[64] = "AT+MLWULDATA=9,000000000000000000\r\n";
 
  buff[0] = 0xFF;
  buff[1] = 0xFE;
  buff[2] = 0x01;
  buff[3] = MESSAGE20;
  buff[4] = 0;
  buff[5] = 0;
  buff[6] = 0;
  buff[7] = 1;
  buff[8] = Upgrade.ResultCode;
  
  crc16 = CRC16(buff,9);
  buff[4] = (crc16>>8)&0xFF;
  buff[5] = crc16&0xFF;
  
  for(i = 0;i < 9;i++)    
  {
    data[15+2*i] = Int_to_ASCLL(buff[i]/0x10);
    data[16+2*i] = Int_to_ASCLL(buff[i]%0x10);  
  }
  
  Uart2_Send(data,35);
}
/*********************************************************************************
 Function:      //
 Description:   //设备主动请求升级包
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void SendUpgradeMessage21(void)
{
  unsigned char i = 0;
  unsigned short crc16 = 0;
  unsigned char buff[26] = {0};
  uint8_t data[70] = "AT+MLWULDATA=26,0000000000000000000000000000000000000000000000000000\r\n"; 

  buff[0] = 0xFF;
  buff[1] = 0xFE;
  buff[2] = 0x01;
  buff[3] = MESSAGE21;
  buff[4] = 0;
  buff[5] = 0;
  buff[6] = 0;
  buff[7] = 18;
  memcpy(&buff[8],Upgrade.Version,16);
  buff[24] = Upgrade.PackageNum/256;
  buff[25] = Upgrade.PackageNum%256;
  
  crc16 = CRC16(buff,26);
  buff[4] = (crc16>>8)&0xFF;
  buff[5] = crc16&0xFF;
  
  for(i = 0;i < 26;i++)    
  {
    data[16+2*i] = Int_to_ASCLL(buff[i]/0x10);
    data[17+2*i] = Int_to_ASCLL(buff[i]%0x10); 
  }
  
  Uart2_Send(data,70);
}
/*********************************************************************************
 Function:      //
 Description:   //设备主动上报升级包下载状态
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void SendUpgradeMessage22(void)
{
 unsigned char i = 0;
  unsigned short crc16 = 0;
  unsigned char buff[10] = {0};
  uint8_t data[64] = "AT+MLWULDATA=9,000000000000000000\r\n";
    
  buff[0] = 0xFF;
  buff[1] = 0xFE;
  buff[2] = 0x01;
  buff[3] = MESSAGE22;
  buff[4] = 0;
  buff[5] = 0;
  buff[6] = 0;
  buff[7] = 1;
  buff[8] = Upgrade.ResultCode;
  
  crc16 = CRC16(buff,9);
  buff[4] = (crc16>>8)&0xFF;
  buff[5] = crc16&0xFF;
  
  for(i = 0;i < 9;i++)    
  {
    data[15+2*i] = Int_to_ASCLL(buff[i]/0x10);
    data[16+2*i] = Int_to_ASCLL(buff[i]%0x10);
  }
  
  Uart2_Send(data,35);
}
/*********************************************************************************
 Function:      //
 Description:   //平台命令执行升级，设备响应报文
 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void SendUpgradeMessage23(void)
{
 unsigned char i = 0;
  unsigned short crc16 = 0;
  unsigned char buff[10] = {0};
  uint8_t data[64] = "AT+MLWULDATA=9,000000000000000000\r\n";
    
  buff[0] = 0xFF;
  buff[1] = 0xFE;
  buff[2] = 0x01;
  buff[3] = MESSAGE23;
  buff[4] = 0;
  buff[5] = 0;
  buff[6] = 0;
  buff[7] = 1;
  buff[8] = 0;
  
  crc16 = CRC16(buff,9);
  buff[4] = (crc16>>8)&0xFF;
  buff[5] = crc16&0xFF;
  
  for(i = 0;i < 9;i++)    
  {
    data[15+2*i] = Int_to_ASCLL(buff[i]/0x10);
    data[16+2*i] = Int_to_ASCLL(buff[i]%0x10);
  }
  
  Uart2_Send(data,35);
}
/*********************************************************************************
 Function:      //
 Description:   //升级超时回调函数

 Input:         //
                //
 Output:        //
 Return:      	//
 Others:        //
*********************************************************************************/
void Upgrade_TimeOut_CallBack(void)
{
  if(Upgrade.TimeoutCounter != 0)//判断次数是否超时
  {
    Upgrade.Incident_Pend = TRUE;
    Upgrade.TimeoutCounter--;
  }
  else
  { 
    Upgrade.PackageNum = 0;          
    Upgrade.ProgramAddr = 0; 
    Upgrade.Process = IDLE;
    Save_Upgrade_Info();

    MCU_DeInit();
  }
  
}
/*********************************************************************************
 Function:      //
 Description:   //升级延时回调函数
 Input:         //
                //
 Output:        //
 Return:        //
 Others:        //
*********************************************************************************/
void Upgrade_Delay_CallBack(void)
{
  Upgrade.Incident_Pend = TRUE;//标记挂起
}