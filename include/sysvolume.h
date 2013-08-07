#ifndef _sysvolume_h_
#define _sysvolume_h_

// 获取/设置系统主音量, 取值范围: 0-0xFFFF
// 高8位为左声道, 低8位为右声道
int GetMasterVolume(int* v);
int SetMasterVolume(int v);

// 获取/设置系统主音量静音, 1-静音, 0-取消静音
int SetMasterVolumeMute(int mute);
int GetMasterVolumeMute(int* mute);

// 获取/设置系统录音音量, 取值范围：0-0xFFFF
// 高8位为左声道, 低8位为右声道
int GetRecordVolume(int* v);
int SetRecordVolume(int v);

// 获取/设置系统录音音量静音, 1-静音, 0-取消静音
int GetRecordVolumeMute(int* mute);
int SetRecordVolumeMute(int mute);

#endif /* !_sysvolume_h_ */
