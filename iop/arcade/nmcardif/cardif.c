
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */
/* mncardif.c */

int start(void)
{
    int iVar1;
    int iVar2;
    sceSifCmdData *psVar3;
    int iVar4;
    ThreadParam T;
    uint16_t local_20;
    int local_1c;
    uint8_t auStack_18 [8];
    
    printf("System246 card IF module\n");
    T.entry = cardIfMainThread;
    T.initPriority = 0x5a;
    T.attr = 0x2000000;
    T.stackSize = 0x800;
    T.option = 0;
    local_20 = _DAT_b0700000;
    iVar1 = CreateThread(&T);
    T.entry = sensorReadThread;
    T.initPriority = 0x59;
    T.attr = 0x2000000;
    T.stackSize = 0x400;
    T.option = 0;
    iVar2 = CreateThread(&T);
    memset(&cardIfWork,0,0x420);
    DAT_00001300 = iVar1;
    FlushDcache();
    sceSifInitCmd();
    CpuSuspendIntr(&local_1c);
    psVar3 = sceSifSetCmdBuffer(&cmdbuffer.20,0x10);
    if (psVar3 != (sceSifCmdData *)0x0) {
      sceSifSetCmdBuffer(psVar3,0x10);
    }
    sceSifAddCmdHandler(0,cmdReqFuncFromEE,&cardIfWork);
    CpuResumeIntr(local_1c);
    acUartGetAttr(&attrData);
    attrData.ua_fifo = 8;
    printf("speed:%d\n",attrData.ua_speed);
    printf("loopback:%d\n",attrData.ua_loopback);
    printf("fifo:%d\n",attrData.ua_fifo);
    acUartSetAttr(&attrData);
    DelayThread(100000);
    do {
      iVar4 = acUartRead(auStack_18,4);
    } while (0 < iVar4);
    if ((((0 < iVar2) && (iVar2 = StartThread(iVar2,0), iVar2 == 0)) && (0 < iVar1)) &&
       (iVar1 = StartThread(iVar1,0), iVar1 == 0)) {
      return MODULE_RESIDENT_END;
    }
    return MODULE_NO_RESIDENT_END;
}


void sensorReadThread(void)
{
  uint uVar1;
  int local_18 [2];
  
  local_18[0] = GetThreadId();
  DAT_00001352 = 0xffff;
  DAT_00001350 = 0xffff;
LAB_000006c4:
  WaitVblankEnd();
  if (((((DAT_00001350 ^ _DAT_b0200000) & 0xff) == 0) &&
      (((DAT_00001352 ^ _DAT_b0200002) & 0xff) == 0)) &&
     (((DAT_00001354 ^ _DAT_b0b00000) & 0xff) == 0)) goto code_r0x00000738;
  goto LAB_0000077c;
code_r0x00000738:
  if (((((DAT_00001356 ^ _DAT_b0b00002) & 0xff) != 0) ||
      (((DAT_00001358 ^ _DAT_b0b00004) & 0xff) != 0)) || (DAT_000012fc == 0)) {
LAB_0000077c:
    DAT_00001350 = _DAT_b0200000;
    DAT_00001352 = _DAT_b0200002;
    DAT_00001354 = _DAT_b0b00000;
    DAT_00001356 = _DAT_b0b00002;
    DAT_00001358 = _DAT_b0b00004;
    DAT_0000135c = _DAT_b2418006 & 0xff;
    uVar1 = sceSifSendCmdIntr(1,&portValue,0x3c,(void *)0x0,(void *)0x0,0,cbSensorSendFinished,
                              local_18);
    if (uVar1 == 0) {
      printf("sceSifSendCmdIntr Error\n");
    }
    else {
      SleepThread();
    }
  }
  goto LAB_000006c4;
}


