/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "PlayerControl.hxx"
#include "Idle.hxx"
#include "Song.hxx"
#include "DecoderControl.hxx"

#include <cmath>

#include <assert.h>

PlayerControl::PlayerControl(unsigned _buffer_chunks,
			     unsigned _buffered_before_play)
	:buffer_chunks(_buffer_chunks),
	 buffered_before_play(_buffered_before_play),
	 command(PlayerCommand::NONE),
	 state(PlayerState::STOP),
	 error_type(PlayerError::NONE),
	 tagged_song(nullptr),
	 next_song(nullptr),
	 cross_fade_seconds(0),
	 mixramp_db(0),
#if defined(__GLIBCXX__) && !defined(_GLIBCXX_USE_C99_MATH_TR1)
	 /* workaround: on MacPorts, this option is disabled on gcc47,
	    and therefore std::nanf() is not available */
	 mixramp_delay_seconds(nanf("")),
#else
	 mixramp_delay_seconds(std::nanf("")),
#endif
	 total_play_time(0),
	 border_pause(false)
{
}

PlayerControl::~PlayerControl()
{
	if (next_song != nullptr)
		next_song->Free();

	if (tagged_song != nullptr)
		tagged_song->Free();
}

void
PlayerControl::Play(Song *song)
{
	assert(song != nullptr);

	Lock();

	if (state != PlayerState::STOP)
		SynchronousCommand(PlayerCommand::STOP);

	assert(next_song == nullptr);

	EnqueueSongLocked(song);

	assert(next_song == nullptr);

	Unlock();
}

void
PlayerControl::Cancel()
{
	LockSynchronousCommand(PlayerCommand::CANCEL);
	assert(next_song == nullptr);
}

void
PlayerControl::Stop()
{
	LockSynchronousCommand(PlayerCommand::CLOSE_AUDIO);
	assert(next_song == nullptr);

	idle_add(IDLE_PLAYER);
}

void
PlayerControl::UpdateAudio()
{
	LockSynchronousCommand(PlayerCommand::UPDATE_AUDIO);
}

void
PlayerControl::Kill()
{
	assert(thread.IsDefined());

	LockSynchronousCommand(PlayerCommand::EXIT);
	thread.Join();

	idle_add(IDLE_PLAYER);
}

void
PlayerControl::PauseLocked()
{
	if (state != PlayerState::STOP) {
		SynchronousCommand(PlayerCommand::PAUSE);
		idle_add(IDLE_PLAYER);
	}
}

void
PlayerControl::Pause()
{
	Lock();
	PauseLocked();
	Unlock();
}

void
PlayerControl::SetPause(bool pause_flag)
{
	Lock();

	switch (state) {
	case PlayerState::STOP:
		break;

	case PlayerState::PLAY:
		if (pause_flag)
			PauseLocked();
		break;

	case PlayerState::PAUSE:
		if (!pause_flag)
			PauseLocked();
		break;
	}

	Unlock();
}

void
PlayerControl::SetBorderPause(bool _border_pause)
{
	Lock();
	border_pause = _border_pause;
	Unlock();
}

player_status
PlayerControl::GetStatus()
{
	player_status status;

	Lock();
	SynchronousCommand(PlayerCommand::REFRESH);

	status.state = state;

	if (state != PlayerState::STOP) {
		status.bit_rate = bit_rate;
		status.audio_format = audio_format;
		status.total_time = total_time;
		status.elapsed_time = elapsed_time;
	}

	Unlock();

	return status;
}

void
PlayerControl::SetError(PlayerError type, Error &&_error)
{
	assert(type != PlayerError::NONE);
	assert(_error.IsDefined());

	error_type = type;
	error = std::move(_error);
}

void
PlayerControl::ClearError()
{
	Lock();

	if (error_type != PlayerError::NONE) {
	    error_type = PlayerError::NONE;
	    error.Clear();
	}

	Unlock();
}

void
PlayerControl::LockSetTaggedSong(const Song &song)
{
	Lock();
	if (tagged_song != nullptr)
		tagged_song->Free();
	tagged_song = song.DupDetached();
	Unlock();
}

void
PlayerControl::ClearTaggedSong()
{
	if (tagged_song != nullptr) {
		tagged_song->Free();
		tagged_song = nullptr;
	}
}

void
PlayerControl::EnqueueSong(Song *song)
{
	assert(song != nullptr);

	Lock();
	EnqueueSongLocked(song);
	Unlock();
}

bool
PlayerControl::Seek(Song *song, float seek_time)
{
	assert(song != nullptr);

	Lock();

	if (next_song != nullptr)
		next_song->Free();

	next_song = song;
	seek_where = seek_time;
	SynchronousCommand(PlayerCommand::SEEK);
	Unlock();

	assert(next_song == nullptr);

	idle_add(IDLE_PLAYER);

	return true;
}

void
PlayerControl::SetCrossFade(float _cross_fade_seconds)
{
	if (_cross_fade_seconds < 0)
		_cross_fade_seconds = 0;
	cross_fade_seconds = _cross_fade_seconds;

	idle_add(IDLE_OPTIONS);
}

void
PlayerControl::SetMixRampDb(float _mixramp_db)
{
	mixramp_db = _mixramp_db;

	idle_add(IDLE_OPTIONS);
}

void
PlayerControl::SetMixRampDelay(float _mixramp_delay_seconds)
{
	mixramp_delay_seconds = _mixramp_delay_seconds;

	idle_add(IDLE_OPTIONS);
}
