#!/usr/bin/env python3
"""
MIDI to .chart converter for Clone Hero / PSP Hero.
Extracts guitar tracks and tempo maps from Guitar Hero / Rock Band / Phase Shift MIDI files.
"""

import sys
import os
import struct

def parse_varlen(data, p):
    val = 0
    while True:
        b = data[p]
        p += 1
        val = (val << 7) | (b & 0x7F)
        if not (b & 0x80):
            break
    return val, p

def mid2chart(midi_path, out_chart_path, song_name="Song"):
    with open(midi_path, "rb") as f:
        data = f.read()

    if data[:4] != b"MThd":
        print(f"Error: {midi_path} is not a valid MIDI file")
        return False

    hdr_len, fmt, ntracks, division = struct.unpack(">IHHH", data[4:14])

    tracks = []
    idx = 14
    for t in range(ntracks):
        if idx >= len(data) or data[idx:idx+4] != b"MTrk":
            break
        track_len, = struct.unpack(">I", data[idx+4:idx+8])
        trk_bytes = data[idx+8:idx+8+track_len]
        tracks.append(trk_bytes)
        idx += 8 + track_len

    sync_events = []
    events_section = []

    diff_notes = {
        "EasySingle": {},
        "MediumSingle": {},
        "HardSingle": {},
        "ExpertSingle": {}
    }
    diff_sp = {
        "EasySingle": [],
        "MediumSingle": [],
        "HardSingle": [],
        "ExpertSingle": []
    }

    # MIDI note mappings for GH/RB standard
    note_map = {}
    for f in range(5):
        note_map[60 + f] = ("EasySingle", f)
        note_map[72 + f] = ("MediumSingle", f)
        note_map[84 + f] = ("HardSingle", f)
        note_map[96 + f] = ("ExpertSingle", f)

    # First pass: identify track names
    track_names = {}
    for trk_idx, trk_bytes in enumerate(tracks):
        p = 0
        cur_tick = 0
        running_status = 0
        while p < len(trk_bytes):
            delta, p = parse_varlen(trk_bytes, p)
            cur_tick += delta
            status = trk_bytes[p]
            if status & 0x80:
                p += 1
                running_status = status
            else:
                status = running_status
            if status == 0xFF:
                meta_type = trk_bytes[p]; p += 1
                length, p = parse_varlen(trk_bytes, p)
                meta_data = trk_bytes[p:p+length]; p += length
                if meta_type == 0x03:
                    name = meta_data.decode("latin1", errors="ignore").strip()
                    track_names[trk_idx] = name
                    break
            elif status in (0xF0, 0xF7):
                length, p = parse_varlen(trk_bytes, p)
                p += length
            elif (status & 0xF0) in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                p += 2
            elif (status & 0xF0) in (0xC0, 0xD0):
                p += 1

    # Locate guitar track
    guitar_track_idx = -1
    for trk_idx, name in track_names.items():
        if name.upper() in ("PART GUITAR", "T1 GEMS", "GUITAR"):
            guitar_track_idx = trk_idx
            break

    if guitar_track_idx == -1:
        for trk_idx, name in track_names.items():
            if "GUITAR" in name.upper() or "LEAD" in name.upper():
                guitar_track_idx = trk_idx
                break

    if guitar_track_idx == -1 and len(tracks) > 1:
        guitar_track_idx = 1

    # Second pass: parse tempo, events, and notes
    for trk_idx, trk_bytes in enumerate(tracks):
        p = 0
        cur_tick = 0
        running_status = 0
        active_notes = {}
        is_guitar_track = (trk_idx == guitar_track_idx)

        while p < len(trk_bytes):
            delta, p = parse_varlen(trk_bytes, p)
            cur_tick += delta

            status = trk_bytes[p]
            if status & 0x80:
                p += 1
                running_status = status
            else:
                status = running_status

            msg_type = status & 0xF0

            if status == 0xFF:
                meta_type = trk_bytes[p]; p += 1
                length, p = parse_varlen(trk_bytes, p)
                meta_data = trk_bytes[p:p+length]; p += length

                if meta_type == 0x51 and len(meta_data) == 3: # Tempo in microsec/quarter
                    tempo_us = (meta_data[0] << 16) | (meta_data[1] << 8) | meta_data[2]
                    bpm_milli = int(round(60000000000.0 / tempo_us))
                    sync_events.append((cur_tick, "B", bpm_milli))
                elif meta_type == 0x58 and len(meta_data) >= 2: # Time signature
                    num = meta_data[0]
                    sync_events.append((cur_tick, "TS", num))
                elif (meta_type == 0x01 or meta_type == 0x05) and (trk_idx == 0 or "EVENT" in track_names.get(trk_idx, "").upper()):
                    txt = meta_data.decode("latin1", errors="ignore").strip()
                    if txt.startswith("[section ") or txt.startswith("[prc_") or txt.startswith("section "):
                        events_section.append((cur_tick, f'E "section {txt.strip("[]")}"'))

            elif status in (0xF0, 0xF7):
                length, p = parse_varlen(trk_bytes, p)
                p += length

            elif msg_type == 0x90: # Note On
                pitch = trk_bytes[p]; vel = trk_bytes[p+1]; p += 2
                if vel > 0:
                    active_notes[pitch] = (cur_tick, vel)
                else: # Note Off via vel == 0
                    if is_guitar_track and pitch in active_notes:
                        start_tick, _ = active_notes.pop(pitch)
                        dur = max(0, cur_tick - start_tick)
                        if pitch in note_map:
                            diff, fret = note_map[pitch]
                            if start_tick not in diff_notes[diff]:
                                diff_notes[diff][start_tick] = []
                            diff_notes[diff][start_tick].append((fret, dur))
                        elif pitch == 116: # Star Power
                            for d in ("EasySingle", "MediumSingle", "HardSingle", "ExpertSingle"):
                                diff_sp[d].append((start_tick, dur))
            elif msg_type == 0x80: # Note Off
                pitch = trk_bytes[p]; vel = trk_bytes[p+1]; p += 2
                if is_guitar_track and pitch in active_notes:
                    start_tick, _ = active_notes.pop(pitch)
                    dur = max(0, cur_tick - start_tick)
                    if pitch in note_map:
                        diff, fret = note_map[pitch]
                        if start_tick not in diff_notes[diff]:
                            diff_notes[diff][start_tick] = []
                        diff_notes[diff][start_tick].append((fret, dur))
                    elif pitch == 116: # Star Power
                        for d in ("EasySingle", "MediumSingle", "HardSingle", "ExpertSingle"):
                            diff_sp[d].append((start_tick, dur))
            elif msg_type in (0xA0, 0xB0, 0xE0):
                p += 2
            elif msg_type in (0xC0, 0xD0):
                p += 1

    # Format into .chart structure
    lines = []
    lines.append("[Song]")
    lines.append("{")
    lines.append(f'  Name = "{song_name}"')
    lines.append(f"  Resolution = {division}")
    lines.append("  Offset = 0")
    lines.append("}")

    lines.append("[SyncTrack]")
    lines.append("{")
    sync_events.sort(key=lambda x: x[0])
    seen_sync = set()
    dedup_sync = []
    for ev in sync_events:
        k = (ev[0], ev[1])
        if k not in seen_sync:
            seen_sync.add(k)
            dedup_sync.append(ev)
    if not any(e[0] == 0 and e[1] == "B" for e in dedup_sync):
        lines.append("  0 = B 120000")
    if not any(e[0] == 0 and e[1] == "TS" for e in dedup_sync):
        lines.append("  0 = TS 4")
    for tick, kind, val in dedup_sync:
        lines.append(f"  {tick} = {kind} {val}")
    lines.append("}")

    lines.append("[Events]")
    lines.append("{")
    events_section.sort(key=lambda x: x[0])
    for tick, txt in events_section:
        lines.append(f"  {tick} = {txt}")
    lines.append("}")

    for diff in ["EasySingle", "MediumSingle", "HardSingle", "ExpertSingle"]:
        notes_dict = diff_notes[diff]
        sp_list = diff_sp[diff]
        if not notes_dict:
            continue
        lines.append(f"[{diff}]")
        lines.append("{")
        all_events = []
        for tick, nlist in notes_dict.items():
            for fret, dur in nlist:
                all_events.append((tick, f"N {fret} {dur}"))
        for tick, dur in sp_list:
            all_events.append((tick, f"S 2 {dur}"))
        all_events.sort(key=lambda x: x[0])
        for tick, content in all_events:
            lines.append(f"  {tick} = {content}")
        lines.append("}")

    with open(out_chart_path, "w", encoding="utf-8") as f:
        f.write("\r\n".join(lines) + "\r\n")

    print(f"Generated {out_chart_path} from {midi_path}")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: mid2chart.py <input.mid> <output.chart> [song_name]")
        sys.exit(1)
    sname = sys.argv[3] if len(sys.argv) > 3 else "Song"
    mid2chart(sys.argv[1], sys.argv[2], sname)
