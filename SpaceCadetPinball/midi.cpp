#include "pch.h"
#include "midi.h"


#include "pb.h"
#include "ft_music.h"

#include <condition_variable>
#include <deque>
#include <mutex>


std::vector<Mix_Music*> midi::LoadedTracks{};
Mix_Music* midi::track1, * midi::track2, * midi::track3;
MidiTracks midi::active_track, midi::NextTrack;
int midi::Volume = MIX_MAX_VOLUME;
bool midi::IsPlaying = false, midi::MixOpen = false;

constexpr uint32_t FOURCC(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	return static_cast<uint32_t>((d << 24) | (c << 16) | (b << 8) | a);
}

int ToVariableLen(uint32_t value, uint32_t& dst)
{
	auto count = 1;
	dst = value & 0x7F;

	while ((value >>= 7))
	{
		dst <<= 8;
		dst |= ((value & 0x7F) | 0x80);
		count++;
	}

	return count;
}

// ---- Music thread ----
// Starting MIDI on Windows opens the system synth (Microsoft GS Wavetable) inside Mix_PlayMusic, which can block for
// up to a second; closing it in Mix_HaltMusic can block too. Doing that on the game thread froze the game whenever
// music was switched on or the game was paused/resumed. Now every SDL_mixer music call runs on one worker thread,
// in order; the game thread only queues commands. Turning music off or pausing the game pauses the stream (the synth
// stays open) instead of halting it, so turning it back on just resumes.
namespace
{
	enum class MusicCmdKind { Play, Halt, Pause, Resume, Volume, Quit };

	struct MusicCmd
	{
		MusicCmdKind Kind;
		Mix_Music* Music;
		int Volume;
	};

	std::mutex MusicMutex;
	std::condition_variable MusicCv;
	std::deque<MusicCmd> MusicQueue;
	std::thread MusicThread;
	bool MusicThreadRunning = false;

	void ExecuteMusicCmd(const MusicCmd& cmd)
	{
		switch (cmd.Kind)
		{
		case MusicCmdKind::Play:
			Mix_HaltMusic();
			if (Mix_PlayMusic(cmd.Music, -1) == 0)
				Mix_VolumeMusic(cmd.Volume); // on Windows MIDI volume only sticks during playback
			else
				printf("Music: could not play track (%s)\n", Mix_GetError());
			break;
		case MusicCmdKind::Halt:
			Mix_HaltMusic();
			break;
		case MusicCmdKind::Pause:
			Mix_PauseMusic();
			break;
		case MusicCmdKind::Resume:
			Mix_ResumeMusic();
			Mix_VolumeMusic(cmd.Volume);
			break;
		case MusicCmdKind::Volume:
			Mix_VolumeMusic(cmd.Volume);
			break;
		case MusicCmdKind::Quit:
			break;
		}
	}

	void MusicThreadMain()
	{
		for (;;)
		{
			MusicCmd cmd{};
			{
				std::unique_lock<std::mutex> lock(MusicMutex);
				MusicCv.wait(lock, [] { return !MusicQueue.empty(); });
				cmd = MusicQueue.front();
				MusicQueue.pop_front();
			}
			if (cmd.Kind == MusicCmdKind::Quit)
				return;
			ExecuteMusicCmd(cmd);
		}
	}

	void PostMusicCmd(MusicCmd cmd)
	{
		if (!MusicThreadRunning)
		{
			ExecuteMusicCmd(cmd);
			return;
		}
		{
			std::lock_guard<std::mutex> lock(MusicMutex);
			if (cmd.Kind == MusicCmdKind::Volume)
			{
				// Only the latest volume matters (the slider sends many)
				for (auto it = MusicQueue.begin(); it != MusicQueue.end();)
					it = it->Kind == MusicCmdKind::Volume ? MusicQueue.erase(it) : it + 1;
			}
			else if (cmd.Kind != MusicCmdKind::Quit)
			{
				// Rapid toggling: play/halt/pause/resume still waiting are replaced by the newest state change
				for (auto it = MusicQueue.begin(); it != MusicQueue.end();)
					it = it->Kind != MusicCmdKind::Volume && it->Kind != MusicCmdKind::Quit ? MusicQueue.erase(it) : it + 1;
			}
			MusicQueue.push_back(cmd);
		}
		MusicCv.notify_one();
	}

	void StartMusicThread()
	{
		if (MusicThreadRunning)
			return;
		MusicQueue.clear();
		MusicThread = std::thread(MusicThreadMain);
		MusicThreadRunning = true;
	}

	void StopMusicThread()
	{
		if (!MusicThreadRunning)
			return;
		{
			std::lock_guard<std::mutex> lock(MusicMutex);
			MusicQueue.push_back({MusicCmdKind::Quit, nullptr, 0});
		}
		MusicCv.notify_one();
		MusicThread.join();
		MusicThreadRunning = false;
	}
}

bool midi::Paused = false;

void midi::music_play()
{
	if (!IsPlaying)
	{
		IsPlaying = true;
		if (Paused && active_track != MidiTracks::None && (NextTrack == active_track || NextTrack == MidiTracks::None))
		{
			// Same track as before: resume it where it stopped, the synth is still open
			Paused = false;
			if (MixOpen)
				PostMusicCmd({MusicCmdKind::Resume, nullptr, Volume});
		}
		else
			play_track(NextTrack, true);
		NextTrack = MidiTracks::None;
	}
}

void midi::music_stop()
{
	if (IsPlaying)
	{
		IsPlaying = false;
		NextTrack = active_track;
		if (active_track != MidiTracks::None)
		{
			Paused = true;
			if (MixOpen)
				PostMusicCmd({MusicCmdKind::Pause, nullptr, Volume});
		}
	}
}

void midi::StopPlayback()
{
	Paused = false;
	if (active_track != MidiTracks::None)
	{
		if (MixOpen)
			PostMusicCmd({MusicCmdKind::Halt, nullptr, Volume});
		active_track = MidiTracks::None;
	}
}

int midi::music_init(bool mixOpen, int volume)
{
	MixOpen = mixOpen;
	SetVolume(volume);
	active_track = MidiTracks::None;
	NextTrack = MidiTracks::None;
	IsPlaying = false;
	Paused = false;
	track1 = track2 = track3 = nullptr;

	if (pb::FT2Table)
	{
		// Other Full Tilt tables: files in the table's Music folder (CD audio replacements, played in name order)
		// replace MIDI tracks 1, 2, 3...; any track without a file keeps the table's own MIDI music.
		auto cdFiles = ftmusic::ListAudioFiles(pb::make_path_name("Music"));
		auto prefix = ftmusic::FindMidiPrefix(pb::make_path_name("SOUND"));
		Mix_Music** tracks[] = {&track1, &track2, &track3};
		for (int i = 0; i < 3; i++)
		{
			if (i < static_cast<int>(cdFiles.size()) && MixOpen && !pb::quickFlag)
			{
				if (auto music = Mix_LoadMUS(cdFiles[i].c_str()))
				{
					LoadedTracks.push_back(music);
					*tracks[i] = music;
					printf("Music track %d: %s\n", i + 1, cdFiles[i].c_str());
					continue;
				}
				printf("Music track %d: could not play %s (%s), using MIDI\n", i + 1, cdFiles[i].c_str(), Mix_GetError());
			}
			if (!prefix.empty())
			{
				*tracks[i] = load_track(prefix + std::to_string(i + 1));
				if (*tracks[i])
					printf("Music track %d: MIDI %s%d\n", i + 1, prefix.c_str(), i + 1);
			}
		}
		if (!track1)
			printf("Music: no Music folder files and no MIDI tracks found for this table\n");
	}
	else if (pb::FullTiltMode)
	{
		track1 = load_track("TABA1");
		track2 = load_track("TABA2");
		track3 = load_track("TABA3");

		// FT demo .006 has only one music track, but it is nearly 9 min. long
		if (!track1 && pb::FullTiltDemoMode)
			track1 = load_track("DEMO");
	}
	else
	{
		// 3DPB has only one music track. PINBALL2.MID is a bitmap font, in the same format as PB_MSGFT.bin
		track1 = load_track("PINBALL");
	}

	if (MixOpen && track1)
		StartMusicThread();
	return track1 != nullptr;
}

void midi::music_shutdown()
{
	music_stop();
	StopPlayback();
	StopMusicThread(); // finishes queued commands, so nothing is playing when the tracks are freed
	if (MixOpen)
		Mix_HaltMusic();

	for (auto midi : LoadedTracks)
	{
		Mix_FreeMusic(midi);
	}
	active_track = MidiTracks::None;
	LoadedTracks.clear();
}

void midi::SetVolume(int volume)
{
	Volume = volume;
	if (MixOpen)
		PostMusicCmd({MusicCmdKind::Volume, nullptr, volume});
}

Mix_Music* midi::load_track(std::string fileName)
{
	if (!MixOpen || pb::quickFlag)
		return nullptr;

	if (pb::FullTiltMode)
	{
		// FT sounds are in SOUND subfolder
		fileName.insert(0, 1, PathSeparator);
		fileName.insert(0, "SOUND");
	}

	auto audio = load_track_sub(fileName, true);
	if (!audio)
		audio = load_track_sub(fileName, false);

	if (!audio)
		return nullptr;

	LoadedTracks.push_back(audio);
	return audio;
}

Mix_Music* midi::load_track_sub(std::string fileName, bool isMidi)
{
	// FT has music in two formats, depending on game version: MIDI in 16bit, MIDS in 32bit.
	// 3DPB music is MIDI only.
	Mix_Music* audio = nullptr;
	fileName += isMidi ? ".MID" : ".MDS";
	for (int i = 0; i < 2; i++)
	{
		if (i == 1)
			std::transform(fileName.begin(), fileName.end(), fileName.begin(),
			               [](unsigned char c) { return std::tolower(c); });
		if (isMidi)
		{
			auto filePath = pb::make_path_name(fileName);
			auto fileHandle = fopenu(filePath.c_str(), "rb");
			if (fileHandle)
			{
				fclose(fileHandle);
				auto rw = SDL_RWFromFile(filePath.c_str(), "rb");
				audio = Mix_LoadMUS_RW(rw, 1);
				break;
			}
		}
		else
		{
			auto midi = MdsToMidi(pb::make_path_name(fileName));
			if (midi)
			{
				// Dump converted MIDI file
				/*auto filePath = fileName + ".midi";
				FILE* fileHandle = fopenu(filePath.c_str(), "wb");
				fwrite(midi->data(), 1, midi->size(), fileHandle);
				fclose(fileHandle);*/

				auto rw = SDL_RWFromMem(midi->data(), static_cast<int>(midi->size()));
				audio = Mix_LoadMUS_RW(rw, 1); // This call seems to leak memory no matter what.
				delete midi;
				break;
			}
		}
	}

	return audio;
}

bool midi::play_track(MidiTracks track, bool replay)
{
	auto midi = TrackToMidi(track);
	if (!midi || (!replay && active_track == track))
		return false;

	StopPlayback();

	if (!IsPlaying)
	{
		NextTrack = track;
		return false;
	}

	// Started on the music thread (opening the system MIDI synth can take a moment); volume is set there after it starts
	if (MixOpen)
		PostMusicCmd({MusicCmdKind::Play, midi, Volume});
	active_track = track;
	return true;
}

MidiTracks midi::get_active_track()
{
	if (!IsPlaying)
		return NextTrack;
	else
		return active_track;
}

Mix_Music* midi::TrackToMidi(MidiTracks track)
{
	Mix_Music* midi;
	switch (track)
	{
	default:
	case MidiTracks::None:
		midi = nullptr;
		break;
	case MidiTracks::Track1:
		midi = track1;
		break;
	case MidiTracks::Track2:
		midi = track2;
		break;
	case MidiTracks::Track3:
		midi = track3;
		break;
	}
	return midi;
}

/// <summary>
/// SDL_mixed does not support MIDS. To support FT music, a conversion to MIDI is required.
/// </summary>
/// <param name="file">Path to .MDS file</param>
/// <returns>Vector that contains MIDI file</returns>
std::vector<uint8_t>* midi::MdsToMidi(std::string file)
{
	auto fileHandle = fopenu(file.c_str(), "rb");
	if (!fileHandle)
		return nullptr;

	fseek(fileHandle, 0, SEEK_END);
	auto fileSize = static_cast<uint32_t>(ftell(fileHandle));
	auto buffer = new uint8_t[fileSize];
	auto fileBuf = reinterpret_cast<riff_header*>(buffer);
	fseek(fileHandle, 0, SEEK_SET);
	fread(fileBuf, 1, fileSize, fileHandle);
	fclose(fileHandle);

	int returnCode = 0;
	std::vector<uint8_t>* midiOut = nullptr;
	do
	{
		if (fileSize < 12)
		{
			returnCode = 3;
			break;
		}
		if (fileBuf->Riff != FOURCC('R', 'I', 'F', 'F') ||
			fileBuf->Mids != FOURCC('M', 'I', 'D', 'S') ||
			fileBuf->Fmt != FOURCC('f', 'm', 't', ' '))
		{
			returnCode = 3;
			break;
		}
		if (fileBuf->FileSize > fileSize - 8)
		{
			returnCode = 3;
			break;
		}
		if (fileSize - 12 < 8)
		{
			returnCode = 3;
			break;
		}
		if (fileBuf->FmtSize < 12 || fileBuf->FmtSize > fileSize - 12)
		{
			returnCode = 3;
			break;
		}

		auto streamIdUsed = fileBuf->dwFlags == 0;
		auto dataChunk = reinterpret_cast<riff_data*>(reinterpret_cast<char*>(&fileBuf->dwTimeFormat) + fileBuf->
			FmtSize);
		if (dataChunk->Data != FOURCC('d', 'a', 't', 'a'))
		{
			returnCode = 3;
			break;
		}
		if (dataChunk->DataSize < 4)
		{
			returnCode = 3;
			break;
		}

		auto srcPtr = dataChunk->Blocks;
		std::vector<midi_event> midiEvents{};
		for (auto blockIndex = dataChunk->BlocksPerChunk; blockIndex; blockIndex--)
		{
			auto eventSizeInt = streamIdUsed ? 3 : 2;
			auto eventCount = srcPtr->CbBuffer / (4 * eventSizeInt);

			auto currentTicks = srcPtr->TkStart;
			auto srcPtr2 = reinterpret_cast<uint32_t*>(srcPtr->AData);
			for (auto i = 0u; i < eventCount; i++)
			{
				currentTicks += srcPtr2[0];
				auto event = streamIdUsed ? srcPtr2[2] : srcPtr2[1];
				midiEvents.push_back({currentTicks, event});
				srcPtr2 += eventSizeInt;
			}

			srcPtr = reinterpret_cast<riff_block*>(&srcPtr->AData[srcPtr->CbBuffer]);
		}

		// MIDS events can be out of order in the file
		std::sort(midiEvents.begin(), midiEvents.end(), [](const midi_event& lhs, const midi_event& rhs)
		{
			return lhs.iTicks < rhs.iTicks;
		});

		// MThd chunk
		std::vector<uint8_t>& midiBytes = *new std::vector<uint8_t>();
		midiOut = &midiBytes;
		midi_header header(SwapByteOrderShort(static_cast<uint16_t>(fileBuf->dwTimeFormat)));
		auto headerData = reinterpret_cast<const uint8_t*>(&header);
		midiBytes.insert(midiBytes.end(), headerData, headerData + sizeof header);

		// MTrk chunk
		midi_track track(7);
		auto trackData = reinterpret_cast<const uint8_t*>(&track);
		midiBytes.insert(midiBytes.end(), trackData, trackData + sizeof track);
		auto lengthPos = midiBytes.size() - 4;

		auto prevTime = 0u;
		for (const auto& event : midiEvents)
		{
			assertm(event.iTicks >= prevTime, "MIDS events: negative delta-time");
			uint32_t delta = event.iTicks - prevTime;
			prevTime = event.iTicks;

			// Delta time is in variable quantity, Big Endian
			uint32_t deltaVarLen;
			auto count = ToVariableLen(delta, deltaVarLen);
			auto deltaData = reinterpret_cast<const uint8_t*>(&deltaVarLen);
			midiBytes.insert(midiBytes.end(), deltaData, deltaData + count);

			switch (event.iEvent >> 24)
			{
			case 0:
				{
					// Type 0 - MIDI short message. 3 bytes: xx p1 p2 00, where xx - message, p* - parameters
					// Some of the messages have only one parameter
					auto msgMask = (event.iEvent) & 0xF0;
					auto shortMsg = (msgMask == 0xC0 || msgMask == 0xD0);
					auto eventData = reinterpret_cast<const uint8_t*>(&event.iEvent);
					midiBytes.insert(midiBytes.end(), eventData, eventData + (shortMsg ? 2 : 3));
					break;
				}
			case 1:
				{
					// Type 1 - tempo change, 3 bytes: xx xx xx 01
					// Meta message, set tempo, 3 bytes payload
					const uint8_t metaSetTempo[] = {0xFF, 0x51, 0x03};
					midiBytes.insert(midiBytes.end(), metaSetTempo, metaSetTempo + 3);

					auto eventBE = SwapByteOrderInt(event.iEvent);
					auto eventData = reinterpret_cast<const uint8_t*>(&eventBE) + 1;
					midiBytes.insert(midiBytes.end(), eventData, eventData + 3);
					break;
				}
			default:
				assertm(0, "MIDS events: uknown event");
				break;
			}
		}

		// Meta message, end of track, 0 bytes payload
		const uint8_t metaEndTrack[] = {0x00, 0xFF, 0x2f, 0x00};
		midiBytes.insert(midiBytes.end(), metaEndTrack, metaEndTrack + 4);

		// Set final MTrk size
		auto lengthBE = SwapByteOrderInt(static_cast<uint32_t>(midiBytes.size()) - sizeof header - sizeof track);
		auto lengthData = reinterpret_cast<const uint8_t*>(&lengthBE);
		std::copy_n(lengthData, 4, midiBytes.begin() + lengthPos);
	}
	while (false);

	delete[] buffer;
	if (returnCode && midiOut)
	{
		delete midiOut;
		midiOut = nullptr;
	}

	return midiOut;
}
