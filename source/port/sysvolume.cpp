#if defined(OS_WINDOWS)
#include <Windows.h>
#include <Mmdeviceapi.h>
#include <Endpointvolume.h>
#include <VersionHelpers.h>
#pragma comment(lib, "Winmm.lib")
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>
#endif

#include "port/sysvolume.h"

#if defined(WIN32)

static bool CheckMixerSourceType(UINT mixer, DWORD connections, DWORD sourceType, DWORD* lineId)
{
	MIXERLINE line;
	for(DWORD i=0; i<connections; i++)
	{		
		memset(&line, 0, sizeof(line));
		line.cbStruct = sizeof(line);
		line.dwSource = i;
		if(MMSYSERR_NOERROR != mixerGetLineInfo((HMIXEROBJ)mixer, &line, MIXER_OBJECTF_MIXER|MIXER_GETLINEINFOF_SOURCE))
			continue;

		if(line.dwComponentType == sourceType)
		{
			*lineId = line.dwLineID;
			return true;
		}
	}
	return false;
}

static int GetMixerControl(DWORD componentType, DWORD controlType, DWORD sourceType, UINT* mixer, DWORD* controlId, DWORD* channels)
{
	int r = -1; // not found
	UINT num = mixerGetNumDevs();
	for(UINT i=0; i<num; i++)
	{
		MIXERCAPS caps;
		memset(&caps, 0, sizeof(caps));
		mixerGetDevCaps(i, &caps, sizeof(caps));

		MIXERLINE line;
		memset(&line, 0, sizeof(line));
		line.cbStruct = sizeof(line);
		line.dwComponentType = componentType;
		MMRESULT ret = mixerGetLineInfo((HMIXEROBJ)i, &line, MIXER_OBJECTF_MIXER|MIXER_GETLINEINFOF_COMPONENTTYPE);
		if(MMSYSERR_NOERROR != ret)
		{
			r = -(int)ret;
			continue;
		}

		DWORD lineId = 0;
		if(0!=sourceType && !CheckMixerSourceType(i, line.cConnections, sourceType, &lineId))
			continue;

		MIXERCONTROL ctrl;
		memset(&ctrl, 0, sizeof(MIXERCONTROL));
		ctrl.cbStruct = sizeof(MIXERCONTROL);

		MIXERLINECONTROLS lineCtrls;
		sizeof(lineCtrls, 0, sizeof(lineCtrls));
		lineCtrls.cbStruct = sizeof(MIXERLINECONTROLS);
		lineCtrls.cbmxctrl = sizeof(MIXERCONTROL);
		lineCtrls.dwLineID = lineId;
		lineCtrls.cControls = 1;
		lineCtrls.pamxctrl = &ctrl;
		lineCtrls.dwControlType = controlType;
		ret = mixerGetLineControls((HMIXEROBJ)i, &lineCtrls, MIXER_OBJECTF_MIXER|MIXER_GETLINECONTROLSF_ONEBYTYPE);
		if(MMSYSERR_NOERROR != ret)
		{
			r = -(int)ret;
			continue;
		}

		*controlId = ctrl.dwControlID;
		*channels = line.cChannels;
		*mixer = i;
		return 0;
	}

	return r;
}

static int GetMixerVolume(int* v)
{
	UINT mixer = 0;
	DWORD channels = 0;
	DWORD controlId = 0;
	int r = GetMixerControl(MIXERLINE_COMPONENTTYPE_DST_SPEAKERS, MIXERCONTROL_CONTROLTYPE_VOLUME, 0, &mixer, &controlId, &channels);
	if(r < 0)
		return r;

	if(channels > 2) channels = 1;

	MIXERCONTROLDETAILS_UNSIGNED value[2];
	memset(&value[0], 0, sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	memset(&value[1], 0, sizeof(MIXERCONTROLDETAILS_UNSIGNED));

	MIXERCONTROLDETAILS details;
	memset(&details, 0, sizeof(details));
	details.cbStruct = sizeof(details);
	details.dwControlID = controlId;
	details.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	details.paDetails = value;
	details.cChannels = channels;
	MMRESULT ret = mixerGetControlDetails((HMIXEROBJ)mixer, &details, MIXER_OBJECTF_MIXER|MIXER_GETCONTROLDETAILSF_VALUE);
	if(MMSYSERR_NOERROR != ret)
		return -(int)ret;

	int left = value[0].dwValue*0xFF/0xFFFF;
	*v = 2==channels ? ((left<<8) | value[1].dwValue*0xFF/0xFFFF) : ((left<<8)|left);
	return 0;
}

static int SetMixerVolume(int v)
{
	UINT mixer = 0;
	DWORD channels = 0;
	DWORD controlId = 0;
	int r = GetMixerControl(MIXERLINE_COMPONENTTYPE_DST_SPEAKERS, MIXERCONTROL_CONTROLTYPE_VOLUME, 0, &mixer, &controlId, &channels);
	if(r < 0)
		return r;

	if(channels > 2) channels = 1;

	MIXERCONTROLDETAILS_UNSIGNED value[2];
	memset(&value[0], 0, sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	memset(&value[1], 0, sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	value[0].dwValue = ((v&0xFF00)>>8) * 0xFFFF / 0xFF;
	value[1].dwValue = (v&0xFF) * 0xFFFF / 0xFF;

	MIXERCONTROLDETAILS details;
	memset(&details, 0, sizeof(details));
	details.cbStruct = sizeof(details);
	details.dwControlID = controlId;
	details.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	details.paDetails = value;
	details.cChannels = channels;
	MMRESULT ret = mixerSetControlDetails((HMIXEROBJ)mixer, &details, MIXER_SETCONTROLDETAILSF_VALUE);
	if(MMSYSERR_NOERROR != ret)
		return -(int)ret;
	return 0;
}

static int GetMixerMute(int* mute)
{
	UINT mixer = 0;
	DWORD channels = 0;
	DWORD controlId = 0;
	int r = GetMixerControl(MIXERLINE_COMPONENTTYPE_DST_SPEAKERS, MIXERCONTROL_CONTROLTYPE_MUTE, 0, &mixer, &controlId, &channels);
	if(r < 0)
		return r;

	MIXERCONTROLDETAILS_BOOLEAN value;
	memset(&value, 0, sizeof(MIXERCONTROLDETAILS_BOOLEAN));
	
	MIXERCONTROLDETAILS details;
	memset(&details, 0, sizeof(details));
	details.cbStruct = sizeof(details);
	details.dwControlID = controlId;
	details.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	details.paDetails = &value;
	details.cChannels = 1;
	MMRESULT ret = mixerGetControlDetails((HMIXEROBJ)mixer, &details, MIXER_GETCONTROLDETAILSF_VALUE);
	if(MMSYSERR_NOERROR != ret)
		return -(int)ret;

	*mute = value.fValue;
	return 0;
}

static int SetMixerMute(int mute)
{
	UINT mixer = 0;
	DWORD channels = 0;
	DWORD controlId = 0;
	int r = GetMixerControl(MIXERLINE_COMPONENTTYPE_DST_SPEAKERS, MIXERCONTROL_CONTROLTYPE_MUTE, 0, &mixer, &controlId, &channels);
	if(r < 0)
		return r;

	MIXERCONTROLDETAILS_BOOLEAN value;
	memset(&value, 0, sizeof(MIXERCONTROLDETAILS_BOOLEAN));
	value.fValue = mute;

	MIXERCONTROLDETAILS details;
	memset(&details, 0, sizeof(details));
	details.cbStruct = sizeof(details);
	details.dwControlID = controlId;
	details.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	details.paDetails = &value;
	details.cChannels = 1;
	MMRESULT ret = mixerSetControlDetails((HMIXEROBJ)mixer, &details, MIXER_SETCONTROLDETAILSF_VALUE);
	if(MMSYSERR_NOERROR != ret)
		return -(int)ret;
	return 0;
}

int GetRecordVolume(int* v)
{
	UINT mixer = 0;
	DWORD channels = 0;
	DWORD controlId = 0;
	int r = GetMixerControl(MIXERLINE_COMPONENTTYPE_DST_WAVEIN, MIXERCONTROL_CONTROLTYPE_VOLUME, MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, &mixer, &controlId, &channels);
	if(r < 0)
		return r;

	if(channels > 2) channels = 1;

	MIXERCONTROLDETAILS_UNSIGNED value[2];
	memset(&value[0], 0, sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	memset(&value[1], 0, sizeof(MIXERCONTROLDETAILS_UNSIGNED));

	MIXERCONTROLDETAILS details;
	memset(&details, 0, sizeof(details));
	details.cbStruct = sizeof(details);
	details.dwControlID = controlId;
	details.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	details.paDetails = value;
	details.cChannels = channels;
	MMRESULT ret = mixerGetControlDetails((HMIXEROBJ)mixer, &details, MIXER_GETCONTROLDETAILSF_VALUE);
	if(MMSYSERR_NOERROR != ret)
		return -(int)ret;

	int left = value[0].dwValue*0xFF/0xFFFF;
	*v = 2==channels ? ((left<<8) | value[1].dwValue*0xFF/0xFFFF) : ((left<<8)|left);
	return 0;
}

int SetRecordVolume(int v)
{
	UINT mixer = 0;
	DWORD channels = 0;
	DWORD controlId = 0;
	int r = GetMixerControl(MIXERLINE_COMPONENTTYPE_DST_WAVEIN, MIXERCONTROL_CONTROLTYPE_VOLUME, MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, &mixer, &controlId, &channels);
	if(r < 0)
		return r;

	if(channels > 2) channels = 1;

	MIXERCONTROLDETAILS_UNSIGNED value[2];
	memset(&value[0], 0, sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	memset(&value[1], 0, sizeof(MIXERCONTROLDETAILS_UNSIGNED));
	value[0].dwValue = ((v&0xFF00)>>8) * 0xFFFF / 0xFF;
	value[1].dwValue = (v&0xFF) * 0xFFFF / 0xFF;

	MIXERCONTROLDETAILS details;
	memset(&details, 0, sizeof(details));
	details.cbStruct = sizeof(details);
	details.dwControlID = controlId;
	details.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	details.paDetails = value;
	details.cChannels = channels;
	MMRESULT ret = mixerSetControlDetails((HMIXEROBJ)mixer, &details, MIXER_SETCONTROLDETAILSF_VALUE);
	if(MMSYSERR_NOERROR != ret)
		return -(int)ret;
	return 0;
}

int GetRecordVolumeMute(int* mute)
{
	UINT mixer = 0;
	DWORD channels = 0;
	DWORD controlId = 0;
	int r = GetMixerControl(MIXERLINE_COMPONENTTYPE_DST_WAVEIN, MIXERCONTROL_CONTROLTYPE_MUTE, MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, &mixer, &controlId, &channels);
	if(r < 0)
		return r;

	MIXERCONTROLDETAILS_BOOLEAN value;
	memset(&value, 0, sizeof(MIXERCONTROLDETAILS_BOOLEAN));

	MIXERCONTROLDETAILS details;
	memset(&details, 0, sizeof(details));
	details.cbStruct = sizeof(details);
	details.dwControlID = controlId;
	details.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	details.paDetails = &value;
	details.cChannels = 1;
	MMRESULT ret = mixerGetControlDetails((HMIXEROBJ)mixer, &details, MIXER_GETCONTROLDETAILSF_VALUE);
	if(MMSYSERR_NOERROR != ret)
		return -(int)ret;

	*mute = value.fValue;
	return 0;
}

int SetRecordVolumeMute(int mute)
{
	UINT mixer = 0;
	DWORD channels = 0;
	DWORD controlId = 0;
	int r = GetMixerControl(MIXERLINE_COMPONENTTYPE_DST_WAVEIN, MIXERCONTROL_CONTROLTYPE_MUTE, MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, &mixer, &controlId, &channels);
	if(r < 0)
		return r;

	MIXERCONTROLDETAILS_BOOLEAN value;
	memset(&value, 0, sizeof(MIXERCONTROLDETAILS_BOOLEAN));
	value.fValue = mute;

	MIXERCONTROLDETAILS details;
	memset(&details, 0, sizeof(details));
	details.cbStruct = sizeof(details);
	details.dwControlID = controlId;
	details.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	details.paDetails = &value;
	details.cChannels = 1;
	MMRESULT ret = mixerSetControlDetails((HMIXEROBJ)mixer, &details, MIXER_SETCONTROLDETAILSF_VALUE);
	if(MMSYSERR_NOERROR != ret)
		return -(int)ret;
	return 0;
}

static int QueryAudioEndpointVolumeInterface(IAudioEndpointVolume** i)
{
	HRESULT hr = S_OK;
	IMMDevice* device = NULL;
	IMMDeviceEnumerator* enumerator = NULL;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
	if(SUCCEEDED(hr))
	{
		hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
		if(SUCCEEDED(hr))
		{
			hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)i);
			device->Release();
		}
		enumerator->Release();
	}
	return hr;
}

static int GetEndpointVolume(int* v)
{
	CoInitialize(NULL);

	IAudioEndpointVolume* endpointVolume = NULL;
	int r = QueryAudioEndpointVolumeInterface(&endpointVolume);
	if(0 == r)
	{
		UINT num = 0;
		endpointVolume->GetChannelCount(&num);
		if(2 == num)
		{
			float left = 0.0f;
			float right = 0.0f;
			r = endpointVolume->GetChannelVolumeLevelScalar(0, &left);
			r = endpointVolume->GetChannelVolumeLevelScalar(1, &right);

			int leftV = int(left*0xFF+1.0f/2); // map volume range to (0-0xFF)
			int rightV = int(right*0xFF+1.0f/2); // map volume range to (0-0xFF)
			*v = (leftV << 8) | rightV;
		}
		else
		{
			float f = 0.0f;
			r = endpointVolume->GetMasterVolumeLevelScalar(&f);
			*v = (int)(f*0xFF + 1.0f/2);
			*v = (*v << 8) | *v;
		}
		endpointVolume->Release();
	}
	
	CoUninitialize();
	return r;
}

static int SetEndpointVolume(int v)
{
	CoInitialize(NULL);

	IAudioEndpointVolume* endpointVolume = NULL;
	int r = QueryAudioEndpointVolumeInterface(&endpointVolume);
	if(0 == r)
	{
		UINT num = 0;
		endpointVolume->GetChannelCount(&num);

		int left = (v & 0xFF00) >> 8;
		int right = (v & 0xFF);
		if(num != 2)
		{
			r = endpointVolume->SetMasterVolumeLevelScalar(left*1.0f/0xFF, NULL);
		}
		else
		{
			r = endpointVolume->SetChannelVolumeLevelScalar(0, left*1.0f/0xFF, NULL);
			r = endpointVolume->SetChannelVolumeLevelScalar(1, right*1.0f/0xFF, NULL);
		}
		endpointVolume->Release();
	}

	CoUninitialize();
	return r;
}

static int SetEndpointMute(int mute)
{
	CoInitialize(NULL);

	IAudioEndpointVolume* endpointVolume = NULL;
	int r = QueryAudioEndpointVolumeInterface(&endpointVolume);
	if(0 == r)
	{
		r = endpointVolume->SetMute(1==mute, NULL);
		endpointVolume->Release();
	}

	CoUninitialize();
	return r;
}

static int GetEndpointMute(int* mute)
{
	CoInitialize(NULL);

	IAudioEndpointVolume* endpointVolume = NULL;
	int r = QueryAudioEndpointVolumeInterface(&endpointVolume);
	if(0 == r)
	{
		BOOL v = FALSE;
		r = endpointVolume->GetMute(&v);
		*mute = v ? 1 : 0;
		endpointVolume->Release();
	}

	CoUninitialize();
	return r;
}

int GetMasterVolume(int* v)
{
	if (IsWindowsVistaOrGreater())
	{
		// Windows Vista / Windows Server 2008 / Windows 7
		return GetEndpointVolume(v);
	}
	else
	{
		// 4: Windows NT 4.0, Windows ME, Windows 98, Windows 95
		// 5: Windows Server 2003 R2, Windows Server 2003, Windows XP, Windows 2000
		return GetMixerVolume(v);
	}
}

int SetMasterVolume(int v)
{
	if (IsWindowsVistaOrGreater())
	{
		// Windows Vista / Windows Server 2008 / Windows 7
		return SetEndpointVolume(v);
	}
	else
	{
		// 4: Windows NT 4.0, Windows ME, Windows 98, Windows 95
		// 5: Windows Server 2003 R2, Windows Server 2003, Windows XP, Windows 2000
		return SetMixerVolume(v);
	}
}

int SetMasterVolumeMute(int mute)
{
	if (IsWindowsVistaOrGreater())
	{
		// Windows Vista / Windows Server 2008 / Windows 7
		return SetEndpointMute(mute);
	}
	else
	{
		// 4: Windows NT 4.0, Windows ME, Windows 98, Windows 95
		// 5: Windows Server 2003 R2, Windows Server 2003, Windows XP, Windows 2000
		return SetMixerMute(mute);
	}
}

int GetMasterVolumeMute(int* mute)
{
	if(IsWindowsVistaOrGreater())
	{
		// Windows Vista / Windows Server 2008 / Windows 7
		return GetEndpointMute(mute);
	}
	else
	{
		// 4: Windows NT 4.0, Windows ME, Windows 98, Windows 95
		// 5: Windows Server 2003 R2, Windows Server 2003, Windows XP, Windows 2000
		return GetMixerMute(mute);
	}
}

#else

#define DEVICE_NAME "default"

typedef int (*ElementCallback)(void* param, snd_mixer_elem_t* e);
static int EnumAlsaMixerElement(const char* device, ElementCallback callback, void* param)
{
	snd_mixer_t* mixer = NULL;
	int r = snd_mixer_open(&mixer, 0);
	if(r < 0)
	{
		printf("EnumAlsaMixerElement snd_mixer_open error: %d, %s\n", r, snd_strerror(r));
		return r;
	}

	r = snd_mixer_attach(mixer, device);
	if(r < 0)
	{
		printf("EnumAlsaMixerElement snd_mixer_attach error: %d, %s\n", r, snd_strerror(r));
		snd_mixer_close(mixer);
		return r;
	}

	r = snd_mixer_selem_register(mixer, NULL, NULL);
	if(r < 0)
	{
		printf("EnumAlsaMixerElement snd_mixer_selem_register error: %d, %s\n", r, snd_strerror(r));
		snd_mixer_close(mixer);
		return r;
	}

	r = snd_mixer_load(mixer);
	if(r < 0)
	{
		printf("EnumAlsaMixerElement snd_mixer_load error: %d, %s\n", r, snd_strerror(r));
		snd_mixer_close(mixer);
		return r;
	}

	r = -1;
	for(snd_mixer_elem_t* e=snd_mixer_first_elem(mixer); e; e=snd_mixer_elem_next(e))
	{
		if(SND_MIXER_ELEM_SIMPLE!=snd_mixer_elem_get_type(e) || !snd_mixer_selem_is_active(e))
			continue;

		r = callback(param, e);
		if(r <= 0)
			break;
		r = -1;
	}

	snd_mixer_close(mixer);
	return r;
}

static int SetMasterVolumeCB(void* param, snd_mixer_elem_t* e)
{
	if(0 == snd_mixer_selem_has_playback_volume(e))
		return 1; // continue

	long min = 0;
	long max = 0;
	snd_mixer_selem_get_playback_volume_range(e, &min, &max);

	long left = ((*(int*)param) & 0xFF00) >> 8;
	long right = (*(int*)param) & 0xFF;
	if(left == right)
	{
		left = (left * (max-min) + 0xFF/2)/0xFF; // map (0-0xFF) to volume range
		//printf("mixer playback volume range: %ld-%ld, set-volume: %ld\n", min, max, left);

		snd_mixer_selem_set_playback_volume_all(e, left);
	}
	else
	{
		left = (left * (max-min) + 0xFF/2)/0xFF; // map (0-0xFF) to volume range
		right = (right * (max-min) + 0xFF/2)/0xFF; // map (0-0xFF) to volume range
		//printf("mixer playback volume range: %ld-%ld, set-front-left: %ld, set-front-right: %ld\n", min, max, left, right);

		snd_mixer_selem_set_playback_volume(e, SND_MIXER_SCHN_FRONT_LEFT, left);
		snd_mixer_selem_set_playback_volume(e, SND_MIXER_SCHN_FRONT_RIGHT, right);
	}
	return 0;
}

static int GetMasterVolumeCB(void* param, snd_mixer_elem_t* e)
{
	if(0 == snd_mixer_selem_has_playback_volume(e))
		return 1; // continue

	long min = 0;
	long max = 0;
	snd_mixer_selem_get_playback_volume_range(e, &min, &max);

	long left = 0;
	long right = 0;
	snd_mixer_selem_get_playback_volume(e, SND_MIXER_SCHN_FRONT_LEFT, &left);
	snd_mixer_selem_get_playback_volume(e, SND_MIXER_SCHN_FRONT_RIGHT, &right);	
	//printf("mixer playback volume range[%ld-%ld], front-left: %ld, front-right: %ld", min, max, left, right);

	left = (left*0xFF+(max-min)/2)/(max-min); // map volume range to (0-0xFF)
	right = (right*0xFF+(max-min)/2)/(max-min); // map volume range to (0-0xFF)

	*(int*)param = (left<<8) | right;
	//printf(" volume: 0x%0X\n", *(int*)param);
	return 0;
}

static int SetRecordVolumeCB(void* param, snd_mixer_elem_t* e)
{
	if(0 == snd_mixer_selem_has_capture_volume(e))
		return 1; // continue

	long min = 0;
	long max = 0;
	snd_mixer_selem_get_capture_volume_range(e, &min, &max);

	long left = ((*(int*)param) & 0xFF00) >> 8;
	long right = (*(int*)param) & 0xFF;
	if(left == right)
	{
		left = (left * (max-min) + 0xFF/2)/0xFF; // map (0-0xFF) to volume range
		//printf("mixer capture volume range: %ld-%ld, set-volume: %ld\n", min, max, left);

		snd_mixer_selem_set_capture_volume_all(e, left);
	}
	else
	{
		left = (left * (max-min) + 0xFF/2)/0xFF; // map (0-0xFF) to volume range
		right = (right * (max-min) + 0xFF/2)/0xFF; // map (0-0xFF) to volume range
		//printf("mixer capture volume range: %ld-%ld, set-front-left: %ld, set-front-right: %ld\n", min, max, left, right);

		snd_mixer_selem_set_capture_volume(e, SND_MIXER_SCHN_FRONT_LEFT, left);
		snd_mixer_selem_set_capture_volume(e, SND_MIXER_SCHN_FRONT_RIGHT, right);
	}
	return 0;
}

static int GetRecordVolumeCB(void* param, snd_mixer_elem_t* e)
{
	if(0 == snd_mixer_selem_has_capture_volume(e))
		return 1; // continue

	long min = 0;
	long max = 0;
	snd_mixer_selem_get_capture_volume_range(e, &min, &max);

	long left = 0;
	long right = 0;
	snd_mixer_selem_get_capture_volume(e, SND_MIXER_SCHN_FRONT_LEFT, &left);
	snd_mixer_selem_get_capture_volume(e, SND_MIXER_SCHN_FRONT_RIGHT, &right);
	//printf("mixer capture volume range[%ld-%ld], front-left: %ld, front-right: %ld", min, max, left, right);

	left = (left*0xFF+(max-min)/2)/(max-min); // map volume range to (0-0xFF)
	right = (right*0xFF+(max-min)/2)/(max-min); // map volume range to (0-0xFF)

	*(int*)param = (left<<8) | right;
	//printf(" volume: 0x%0X\n", *(int*)param);
	return 0;
}

static int SetMasterVolumeMuteCB(void* param, snd_mixer_elem_t* e)
{
	if(0 == snd_mixer_selem_has_playback_switch(e))
		return 1; // continue

	int onoff = 1==*(int*)param ? 0 : 1;
	snd_mixer_selem_set_playback_switch_all(e, onoff);
	return 0;
}

static int GetMasterVolumeMuteCB(void* param, snd_mixer_elem_t* e)
{
	if(0 == snd_mixer_selem_has_playback_switch(e))
		return 1; // continue

	int onoff = 0;
	snd_mixer_selem_get_playback_switch(e, SND_MIXER_SCHN_FRONT_LEFT, &onoff);
	*(int*)param = 1==onoff ? 0 : 1;
	return 0;
}

static int SetRecordVolumeMuteCB(void* param, snd_mixer_elem_t* e)
{
	if(0 == snd_mixer_selem_has_capture_switch(e))
		return 1; // continue

	int onoff = 1==*(int*)param ? 0 : 1;
	snd_mixer_selem_set_capture_switch_all(e, onoff);
	return 0;
}

static int GetRecordVolumeMuteCB(void* param, snd_mixer_elem_t* e)
{
	if(0 == snd_mixer_selem_has_capture_switch(e))
		return 1; // continue

	int onoff = 0;
	snd_mixer_selem_get_capture_switch(e, SND_MIXER_SCHN_FRONT_LEFT, &onoff);
	*(int*)param = 1==onoff ? 0 : 1;
	return 0;
}

int SetMasterVolume(int v)
{
	return EnumAlsaMixerElement(DEVICE_NAME, SetMasterVolumeCB, &v);
}

int GetMasterVolume(int* v)
{
	return EnumAlsaMixerElement(DEVICE_NAME, GetMasterVolumeCB, v);
}

int SetRecordVolume(int v)
{
	return EnumAlsaMixerElement(DEVICE_NAME, SetRecordVolumeCB, &v);
}

int GetRecordVolume(int* v)
{
	return EnumAlsaMixerElement(DEVICE_NAME, GetRecordVolumeCB, v);
}

int SetMasterVolumeMute(int mute)
{
	return EnumAlsaMixerElement(DEVICE_NAME, SetMasterVolumeMuteCB, &mute);
}

int GetMasterVolumeMute(int* mute)
{
	return EnumAlsaMixerElement(DEVICE_NAME, GetMasterVolumeMuteCB, mute);
}

int SetRecordVolumeMute(int mute)
{
	return EnumAlsaMixerElement(DEVICE_NAME, SetRecordVolumeMuteCB, &mute);
}

int GetRecordVolumeMute(int* mute)
{
	return EnumAlsaMixerElement(DEVICE_NAME, GetRecordVolumeMuteCB, mute);
}

#endif
