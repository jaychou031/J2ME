#ifndef J2ME_PHONEME_AUDIO_H
#define J2ME_PHONEME_AUDIO_H

void j2me_phoneme_audio_pump(void);
void j2me_phoneme_audio_stop(void);
void j2me_phoneme_audio_adjust_master_volume(int delta);
void j2me_phoneme_audio_toggle_master_mute(void);
unsigned int j2me_phoneme_audio_master_volume(void);
int j2me_phoneme_audio_master_muted(void);
unsigned int j2me_phoneme_audio_master_change_count(void);
unsigned int j2me_phoneme_audio_start_count(void);
unsigned int j2me_phoneme_audio_midi_start_count(void);
unsigned int j2me_phoneme_audio_wav_start_count(void);
unsigned int j2me_phoneme_audio_tone_count(void);
unsigned int j2me_phoneme_audio_stop_count(void);
unsigned int j2me_phoneme_audio_note_count(void);
unsigned int j2me_phoneme_audio_write_count(void);
unsigned int j2me_phoneme_audio_write_bytes(void);
unsigned int j2me_phoneme_audio_write_errors(void);
unsigned int j2me_phoneme_audio_ready_misses(void);

#endif
