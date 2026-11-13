/* sound.h -- sound mixer. Notes: Research/notes/headers.md */
#ifndef SOUND_H
#define SOUND_H

#include <MacTypes.h>
#include <Sound.h>

#define kMaxVoiceVolume		128
#define kMaxMasterVolume	256

typedef struct SoundRequest SoundRequest;
typedef void (*SoundCallback)(long message, long id, SoundRequest *req);

#pragma options align=mac68k

struct SoundRequest {
	Ptr				snd;
	long			id;
	Fixed			pitch;
	SoundCallback	callback;
	long			extra;
	unsigned short	priority;
	unsigned short	volLeft;
	unsigned short	volRight;
	unsigned short	reserved;
};

#pragma options align=reset

OSErr InstallCallbackPtr(Ptr p, long proc);
OSErr BootSoundSubsystem(unsigned short maxVoices, Boolean stereo, unsigned long sampleRate);
void TeardownSoundSubsystem(void);
void SetMasterVolume(unsigned short volume);
unsigned short GetMasterVolume(void);
long TriggerSoundPlay(Ptr snd, unsigned short priority, short volume);
long EnqueueSoundVoice(SoundRequest *req);
void FlushMixQueueEntries(long snd);
unsigned short CountMatchingSoundVoices(long snd);
Ptr LoadSndResource(short id);
void RemoveMixerVoiceAt(short index);

#endif
