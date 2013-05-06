#ifndef __TVIN_APIS_H
#define __TVIN_APIS_H

#include <linux/tvin/tvin.h>

#define CC_HIST_GRAM_BUF_SIZE   (64)
// ***************************************************************************
// *** TvinApi function definition *******************************************
// ***************************************************************************




extern int AFE_DeviceIOCtl(int request, ...);
extern int TvinApi_OpenPPMGRModule(void);
extern int TvinApi_ClosePPMGRModule(void);
extern int TvinApi_OnoffVScaler(int onOff);
extern int TvinApi_Send3DCommand(int commd);
extern int TvinApi_Set3DOverscan(int top, int left);
extern int TvinApi_OpenVDINModule(int selVDIN);
extern int TvinApi_CloseVDINModule(int selVDIN);
extern int TvinApi_IsVDINModuleOpen(int selVDIN);
extern int TvinApi_OpenPort(int selVDIN, int sourceId);
extern int TvinApi_ClosePort(int selVDIN);
extern int TvinApi_StartDec(int selVDIN, tvin_parm_t TvinVDINParam);
extern int TvinApi_GetSignalInfo(int selVDIN, tvin_info_t *SignalInfo);
extern int TvinApi_GetHistgram(int *histgram_buf);
extern int TvinApi_StopDec(int selVDIN);
extern int TvinApi_ManualSetPath(char *videopath);
extern int TvinApi_AddTvPath(int selPath);
extern int TvinApi_RmTvPath(void);
extern int TvinApi_RmDefPath(void);
extern int TvinApi_RmPreviewPath(void);
extern int TvinApi_EnableFreeScale(const int osdWidth, const int osdHeight, const int previewX0, const int previewY0, const int previewX1,  const int previewY1);
extern int TvinApi_DisableFreeScale(int mode);
extern int TvinApi_SetDisplayVFreq(int freq);
extern int TvinApi_SetDepthOfField(int setvalue);
extern int TvinApi_Set2D3DDepth(int setvalue);
extern int TvinApi_Set2Dto3D(int on_off);
extern int TvinApi_SetDIBuffMgrMode(int mgr_mode);
extern int TvinApi_SetDICFG(int cfg);
extern int TvinApi_SetPpmgrMode(int mode);
extern int TvinApi_SetDI3DDetc(int enable);
extern int TvinApi_GetDI3DDetc();
extern int TvinApi_GetDICFG();
extern int TvinApi_SetMVCMode(int mode);
extern int TvinApi_GetMVCMode(void);
extern int TvinApi_GetVscalerStatus(void);
extern int TvinApi_SetPpmgrPlatformType(int mode);
extern int TvinApi_SetPpmgrView_mode(int mode);
extern int TvinApi_Set3DOvserScan(int top, int left);
extern int TvinApi_TurnOnBlackBarDetect(int isEnable);
extern int TvinApi_LoadHdcpKey(unsigned char *hdcpkeybuff);
extern int TvinApi_OpenAFEModule(void);
extern void TvinApi_CloseAFEModule(void);
extern int TvinApi_SetVGAEdid(unsigned char *vgaedid);
extern int TvinApi_GetVGAEdid(unsigned char *vgaedid);
extern int TvinApi_SetVGACurTimingAdj(tvafe_vga_parm_t adjparam);
extern int TvinApi_GetVGACurTimingAdj(tvafe_vga_parm_t *adjparam);
extern int TvinApi_VGAAutoAdj(tvafe_vga_parm_t *adjparam);
extern int TvinApi_SetVGAAutoAdjust(void);
extern int TvinApi_GetVGAAutoAdjustCMDStatus(tvafe_cmd_status_t *Status);
extern int TvinApi_GetYPbPrWSSInfo(tvafe_comp_wss_t *wssinfo);
extern int TvinApi_GetADCGainOffset(tvafe_adc_cal_t *AdcCalValue);
extern int TvinApi_SetADCGainOffset(tvafe_adc_cal_t adcParm);
extern int TvinApi_GetYPbPrADCGainOffset(tvafe_adc_comp_cal_t *AdcCalValue);
extern int TvinApi_SetYPbPrADCGainOffset(tvafe_adc_comp_cal_t adcParm);
extern int TvinApi_ADCAutoCalibration_Old(int typeSel, int rangeSel);
extern int TvinApi_GetMemData(int typeSel);
extern int TvinApi_CVBSLockStatus(void);
extern int TvinApi_SetCVBSStd(int fmt);
extern int TvinApi_ADCAutoCalibration(void);
extern int TvinApi_ADCAutoCalibration(void);
extern int TvinApi_ADCGetPara(unsigned char selwin, struct tvin_parm_s *para);
extern int TvinApi_KeepLastFrame(int enable);
extern int TvinApi_SetBlackOutPolicy(int enable);
extern int TvinApi_SetVideoFreeze(int enable);
extern int TvinApi_SetDIBypasshd(int enable);
extern int TvinApi_SetDIBypassAll(int enable);
extern int TvinApi_SetDIBypassPost(int enable);
extern int TvinApi_SetD2D3Bypass(int enable);
extern int TvinApi_SetHDMIEQConfig(int config);
extern int TvinApi_SetVdinFlag(int flag);
extern int TvinApi_SetRDMA(int enable);
extern int TvinApi_MHL_WorkEnable(int enable);
extern int TvinApi_GetMhlInputMode(void);
extern int TvinApi_SetVdinHVScale(int vdinx, int hscale, int vscale);
extern int TvinApi_SetCompPhase(void);
extern int TvinApi_GetVdinPortSignal(int port);
extern int TvinApi_SetStartDropFrameCn(int count);
extern int TvinApi_SetCompPhaseEnable(int enable);
extern int VDIN_OpenPort(const unsigned char selVDIN, const struct tvin_parm_s *vdinParam);
#endif //__TVIN_APIS_H
