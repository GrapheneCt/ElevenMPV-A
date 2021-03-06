#include <kernel.h>
#include <appmgr.h>
#include <audioout.h>
#include <shellaudio.h>

#include "audio.h"
#include "vitaaudiolib.h"
#include "fs.h"
#include "touch_e.h"
#include "utils.h"

#include "flac.h"
#include "ogg.h"
#include "opus.h"
#include "xm.h"
#include "at3.h"
#include "mp3.h"
#include "sh_common.h"

enum Audio_FileType {
	FILE_TYPE_NONE,
	FILE_TYPE_FLAC,
	FILE_TYPE_MP3,
	FILE_TYPE_OGG,
	FILE_TYPE_OPUS,
	FILE_TYPE_WAV,
	FILE_TYPE_XM,
	FILE_TYPE_AAC,
	FILE_TYPE_ATRAC9,
	FILE_TYPE_ATRAC3
};

enum Audio_BgmMode {
	BGM_MODE_NONE,
	BGM_MODE_NORMAL,
	BGM_MODE_SHC
};

typedef struct {
	int (* init)(const char *path);
	SceUInt32 (* rate)(void);
	SceUInt8 (* channels)(void);
	void (* decode)(void *buf, unsigned int length, void *userdata);
	SceUInt64 (* position)(void);
	SceUInt64 (* length)(void);
	SceUInt64 (* seek)(SceUInt64 index);
	void (* term)(void);
} Audio_Decoder;

static enum Audio_BgmMode bgm_mode = BGM_MODE_NORMAL;
static enum Audio_FileType file_type = FILE_TYPE_NONE;
Audio_Metadata metadata = {0};
static Audio_Decoder decoder = {0}, empty_decoder = {0};
static SceUInt64 seek_position = 0;
static SceBool seek_mode = SCE_FALSE;

SceBool playing = SCE_FALSE, paused = SCE_FALSE;
SceMusicPlayerServicePlayStatusExtension pb_stats;

extern SceUID event_flag_uid;

int sceAppMgrAcquireBgmPortForMusicPlayer();
int sceAppMgrAcquireBgmPortWithPriority(int priority);

static void Audio_Decode(void *buf, unsigned int length, void *userdata) {
	if (playing == SCE_FALSE || paused == SCE_TRUE) {
		short *buf_short = (short *)buf;
		unsigned int count;
		for (count = 0; count < length * 2; count++)
			*(buf_short + count) = 0;
	} 
	else
		(* decoder.decode)(buf, length, userdata);
}

void Audio_ResetBgmMode(void) {
	bgm_mode = BGM_MODE_NONE;
}

int Audio_Init(const char *path) {
	playing = SCE_TRUE;
	paused = SCE_FALSE;
	
	if (!sceClibStrncasecmp(FS_GetFileExt(path), "flac", 4))
		file_type = FILE_TYPE_FLAC;
	else if (!sceClibStrncasecmp(FS_GetFileExt(path), "mp3", 3))
		file_type = FILE_TYPE_MP3;
	else if (!sceClibStrncasecmp(FS_GetFileExt(path), "ogg", 3))
		file_type = FILE_TYPE_OGG;
	else if (!sceClibStrncasecmp(FS_GetFileExt(path), "opus", 4))
		file_type = FILE_TYPE_OPUS;
	else if (!sceClibStrncasecmp(FS_GetFileExt(path), "wav", 3))
		file_type = FILE_TYPE_WAV;
	else if ((!sceClibStrncasecmp(FS_GetFileExt(path), "it", 2)) || (!sceClibStrncasecmp(FS_GetFileExt(path), "mod", 3))
		|| (!sceClibStrncasecmp(FS_GetFileExt(path), "s3m", 3)) || (!sceClibStrncasecmp(FS_GetFileExt(path), "xm", 2)))
		file_type = FILE_TYPE_XM;
	else if (!sceClibStrncasecmp(FS_GetFileExt(path), "at9", 3))
		file_type = FILE_TYPE_ATRAC9;
	else if ((!sceClibStrncasecmp(FS_GetFileExt(path), "m4a", 3)) || (!sceClibStrncasecmp(FS_GetFileExt(path), "aac", 3)))
		file_type = FILE_TYPE_AAC;
	else if ((!sceClibStrncasecmp(FS_GetFileExt(path), "oma", 3)) || (!sceClibStrncasecmp(FS_GetFileExt(path), "aa3", 3)) || (!sceClibStrncasecmp(FS_GetFileExt(path), "at3", 3)))
		file_type = FILE_TYPE_ATRAC3;

	switch(file_type) {
		case FILE_TYPE_FLAC:
			decoder.init = FLAC_Init;
			decoder.rate = FLAC_GetSampleRate;
			decoder.channels = FLAC_GetChannels;
			decoder.decode = FLAC_Decode;
			decoder.position = FLAC_GetPosition;
			decoder.length = FLAC_GetLength;
			decoder.seek = FLAC_Seek;
			decoder.term = FLAC_Term;
			sceKernelSetEventFlag(event_flag_uid, FLAG_ELEVENMPVA_IS_DECODER_USED);
			break;

		case FILE_TYPE_MP3:
			decoder.init = MP3_Init;
			decoder.rate = NULL;
			decoder.channels = NULL;
			decoder.decode = NULL;
			decoder.position = SHC_GetPosition;
			decoder.length = SHC_GetLength;
			decoder.seek = SHC_Seek;
			decoder.term = SHC_Term;
			sceKernelClearEventFlag(event_flag_uid, ~FLAG_ELEVENMPVA_IS_DECODER_USED);
			break;

		case FILE_TYPE_OGG:
			decoder.init = OGG_Init;
			decoder.rate = OGG_GetSampleRate;
			decoder.channels = OGG_GetChannels;
			decoder.decode = OGG_Decode;
			decoder.position = OGG_GetPosition;
			decoder.length = OGG_GetLength;
			decoder.seek = OGG_Seek;
			decoder.term = OGG_Term;
			sceKernelSetEventFlag(event_flag_uid, FLAG_ELEVENMPVA_IS_DECODER_USED);
			break;

		case FILE_TYPE_OPUS:
			decoder.init = OPUS_Init;
			decoder.rate = OPUS_GetSampleRate;
			decoder.channels = OPUS_GetChannels;
			decoder.decode = OPUS_Decode;
			decoder.position = OPUS_GetPosition;
			decoder.length = OPUS_GetLength;
			decoder.seek = OPUS_Seek;
			decoder.term = OPUS_Term;
			sceKernelSetEventFlag(event_flag_uid, FLAG_ELEVENMPVA_IS_DECODER_USED);
			break;

		case FILE_TYPE_WAV:
			decoder.init = SHC_Init;
			decoder.rate = NULL;
			decoder.channels = NULL;
			decoder.decode = NULL;
			decoder.position = SHC_GetPosition;
			decoder.length = SHC_GetLength;
			decoder.seek = SHC_Seek;
			decoder.term = SHC_Term;
			sceKernelClearEventFlag(event_flag_uid, ~FLAG_ELEVENMPVA_IS_DECODER_USED);
			break;

		case FILE_TYPE_XM:
			decoder.init = XM_Init;
			decoder.rate = XM_GetSampleRate;
			decoder.channels = XM_GetChannels;
			decoder.decode = XM_Decode;
			decoder.position = XM_GetPosition;
			decoder.length = XM_GetLength;
			decoder.seek = XM_Seek;
			decoder.term = XM_Term;
			sceKernelSetEventFlag(event_flag_uid, FLAG_ELEVENMPVA_IS_DECODER_USED);
			break;

		case FILE_TYPE_ATRAC9:
			decoder.init = SHC_Init;
			decoder.rate = NULL;
			decoder.channels = NULL;
			decoder.decode = NULL;
			decoder.position = SHC_GetPosition;
			decoder.length = SHC_GetLength;
			decoder.seek = SHC_Seek;
			decoder.term = SHC_Term;
			sceKernelClearEventFlag(event_flag_uid, ~FLAG_ELEVENMPVA_IS_DECODER_USED);
			break;

		case FILE_TYPE_AAC:
			decoder.init = SHC_Init;
			decoder.rate = NULL;
			decoder.channels = NULL;
			decoder.decode = NULL;
			decoder.position = SHC_GetPosition;
			decoder.length = SHC_GetLength;
			decoder.seek = SHC_Seek;
			decoder.term = SHC_Term;
			sceKernelClearEventFlag(event_flag_uid, ~FLAG_ELEVENMPVA_IS_DECODER_USED);
			break;

		case FILE_TYPE_ATRAC3:
			decoder.init = AT3_Init;
			decoder.rate = AT3_GetSampleRate;
			decoder.channels = AT3_GetChannels;
			decoder.decode = AT3_Decode;
			decoder.position = AT3_GetPosition;
			decoder.length = AT3_GetLength;
			decoder.seek = AT3_Seek;
			decoder.term = AT3_Term;
			sceKernelSetEventFlag(event_flag_uid, FLAG_ELEVENMPVA_IS_DECODER_USED);
			break;

		default:
			break;
	}

	if (!Utils_IsDecoderUsed() && bgm_mode != BGM_MODE_SHC) {
		sceAppMgrReleaseBgmPort();
		sceAppMgrAcquireBgmPortWithPriority(0x80);
		bgm_mode = BGM_MODE_SHC;
	}
	else if (Utils_IsDecoderUsed() && bgm_mode != BGM_MODE_NORMAL) {
		sceAppMgrReleaseBgmPort();
		sceAppMgrAcquireBgmPortForMusicPlayer();
		bgm_mode = BGM_MODE_NORMAL;
	}

	(* decoder.init)(path);
	if (Utils_IsDecoderUsed())
		vitaAudioInit((* decoder.rate)(), (* decoder.channels)() == 2? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
	vitaAudioSetChannelCallback(Audio_Decode, NULL);
	return 0;
}

SceBool Audio_IsPaused(void) {
	return paused;
}

void Audio_Pause(void) {
	if (!Utils_IsDecoderUsed() && paused)
		sceMusicPlayerServiceSendEvent(SCE_MUSICCORE_EVENTID_PLAY, 0);
	else if (!Utils_IsDecoderUsed())
		sceMusicPlayerServiceSendEvent(SCE_MUSICCORE_EVENTID_STOP, 0);
	paused = !paused;
}

void Audio_Stop(void) {
	if (!Utils_IsDecoderUsed())
		sceMusicPlayerServiceSendEvent(SCE_MUSICCORE_EVENTID_STOP, 0);
	playing = !playing;
}

SceUInt64 Audio_GetPosition(void) {
	if (seek_mode)
		return (Audio_GetLength() * (seek_position / SEEK_WIDTH_FLOAT));
	else
		return (* decoder.position)();
}

SceUInt64 Audio_GetLength(void) {
	return (* decoder.length)();
}

SceUInt64 Audio_GetPositionSeconds(void) {
	if (!Utils_IsDecoderUsed())
		return (Audio_GetPosition() / 1000);
	else
		return (Audio_GetPosition() / (* decoder.rate)());
}

SceUInt64 Audio_GetLengthSeconds(void) {
	if (!Utils_IsDecoderUsed())
		return (Audio_GetLength() / 1000);
	else
		return (Audio_GetLength() / (*decoder.rate)());
}

SceUInt64 Audio_Seek(void) {
	return (*decoder.seek)(seek_position);
}

void Audio_SetSeekPosition(SceUInt64 index) {
	seek_position = index;
}

void Audio_SetSeekMode(SceBool mode) {
	seek_mode = mode;
}

SceBool Audio_GetSeekMode(void) {
	return seek_mode;
}

void Audio_Term(void) {
	playing = SCE_TRUE;
	paused = SCE_FALSE;

	vitaAudioSetChannelCallback(NULL, NULL); // Clear channel callback
	vitaAudioEndPre();
	sceKernelDelayThread(100 * 1000);
	vitaAudioEnd();
	(* decoder.term)();

	decoder = empty_decoder;
}
