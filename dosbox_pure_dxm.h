/* dosbox_pure_dxm.h — DOS ex Machina's side of the core.
 *
 * The machine around this emulator draws its own BIOS screen, the POST,
 * and clears it when the tube is handed over.  What comes after the clear
 * is this: the System Configurations box, the pauses a real machine took
 * between them, the drive it ran, and "Starting DXM-DOS...".  It belongs
 * in here rather than in a batch file on C: because a batch file prints
 * its lines in a few milliseconds - nothing takes any time, so nothing
 * reads as loading - and because the boot has no business leaving a file
 * lying on the user's disk.
 *
 * It runs as Z:\DXMBIOS.COM, the first line of the autoexec, and prints
 * nothing until the frontend says the tube is showing this screen: the
 * emulator is at its prompt seconds before the POST ends, so a program
 * that started printing at once would be finished before anyone saw it.
 *
 * The frontend is reached through libretro's private environment range.
 * Any other frontend answers false to all of it, DXM_Present() is false,
 * and this program takes itself out of the boot entirely. */
#ifndef DOSBOX_PURE_DXM_H
#define DOSBOX_PURE_DXM_H

#define DXM_ENV_HELLO (RETRO_ENVIRONMENT_PRIVATE | 1) /* bool*    : the frontend is DXM       */
#define DXM_ENV_MHZ   (RETRO_ENVIRONMENT_PRIVATE | 2) /* unsigned*: the turbo display's clock */
#define DXM_ENV_SHOWN (RETRO_ENVIRONMENT_PRIVATE | 3) /* bool*    : the tube shows this screen*/
#define DXM_ENV_DRIVE (RETRO_ENVIRONMENT_PRIVATE | 4) /* unsigned*: run the drive, in ms      */
/* const char**: which MIDI device the machine wants - "auto" for whatever
 * is on C:, "off" for none, or "mt32", "sc55", "sf2" to pick one of them.
 * Without this the core takes whatever ROMs it finds, and the SC-55 in
 * particular emulates a whole second processor whether anything is playing
 * or not, which is not a thing to switch on by accident. */
#define DXM_ENV_MIDI (RETRO_ENVIRONMENT_PRIVATE | 5)
/* bool*: SETUP.  Setting it asks the machine to put its own configuration
 * screen up; reading it says whether the screen is still there.  The
 * program that asks then waits, so DOS is occupied for as long as it is. */
#define DXM_ENV_SETUP (RETRO_ENVIRONMENT_PRIVATE | 6)
/* dxm_catalog_msg*: CATALOG.  The program and the machine's screen talk
 * through one of these; the core fills `op`, the machine the rest.  The
 * struct is the same on both sides (src/dosbox/dosbox.c). */
#define DXM_ENV_CATALOG (RETRO_ENVIRONMENT_PRIVATE | 7)
/* const char**: the drives the machine wants besides C:, a line each as
 * "D=LABEL=/host/folder/".  A catalogue on a letter is one of these. */
#define DXM_ENV_DRIVES (RETRO_ENVIRONMENT_PRIVATE | 8)
struct dxm_catalog_msg
{
	int op;    /* from the core: 0 poll, 1 open, 2 back from an excursion */
	int reply; /* from the machine: 0 stay up, 1 closed, 2 run an excursion */
	int kind;  /* the excursion: 0 a command, 1 a nested COMMAND */
	char drive;
	char dir[80];
	char cmd[80];
	char rescan; /* a drive whose listing the machine changed, or 0 */
};

/* Set the first time the frontend answers, and read from the shell (HELP
 * lists the machine's own commands only when it is the machine in front). */
bool dbp_dxm_present = false;

static bool DXM_Present()
{
	bool yes = false;
	dbp_dxm_present = (environ_cb && environ_cb(DXM_ENV_HELLO, &yes) && yes);
	return dbp_dxm_present;
}

/* NULL when the machine in front is not DXM, in which case the core keeps
 * its own habit of using whatever it finds. */
static const char* DXM_Midi()
{
	const char* want = NULL;
	return (environ_cb && environ_cb(DXM_ENV_MIDI, &want) ? want : NULL);
}

/* The machine's own programs that are ordinary DOS executables rather than
 * callbacks into the core: served from memory on Z:, which is on the PATH,
 * so they are always there and cannot be deleted.  FreeDOS EDIT, from
 * dxm/edit/ (GPL-2.0, source alongside).  Z: is read-only, so EDIT runs
 * on its defaults: it has no EDIT.CFG and cannot write one there. */
#include "dosbox_pure_dxm_edit.h"
static void DXM_RegisterFiles()
{
	if (!DXM_Present()) return;
	VFILE_Register("EDIT.EXE", (Bit8u*)dxm_edit_exe, (Bit32u)sizeof(dxm_edit_exe));
	VFILE_Register("EDIT.HLP", (Bit8u*)dxm_edit_hlp, (Bit32u)sizeof(dxm_edit_hlp));
}

/* The drives besides C:, mounted as the core starts, as plain hard disks on
 * the folders the machine keeps for them: what is installed there stays
 * there, and DOS sees it as it would a second disk. */
static void DXM_MountDrives()
{
	const char* list = NULL;
	if (!DXM_Present() || !environ_cb(DXM_ENV_DRIVES, &list) || !list) return;
	for (const char* p = list; *p;)
	{
		const char* end = strchr(p, '\n');
		std::string line(p, end ? (size_t)(end - p) : strlen(p));
		p = end ? end + 1 : p + line.size();
		/* D=LABEL=/folder/ */
		size_t eq2 = line.find('=', 2);
		if (line.size() < 5 || line[1] != '=' || eq2 == std::string::npos) continue;
		char letter = (char)toupper((unsigned char)line[0]);
		if (letter < 'D' || letter > 'Z' || Drives[letter-'A']) continue;
		std::string label = line.substr(2, eq2 - 2), dir = line.substr(eq2 + 1);
		if (dir.empty()) continue;
		strreplace((char*)dir.c_str(), (CROSS_FILESPLIT == '\\' ? '/' : '\\'), CROSS_FILESPLIT);
		if (dir.back() != CROSS_FILESPLIT) dir += CROSS_FILESPLIT;
		dir_information* dirp = open_directory(dir.c_str());
		if (!dirp) { emuthread_notify(0, LOG_ERROR, "DXM: cannot mount %c: on %s", letter, dir.c_str()); continue; }
		close_directory(dirp);
		localDrive* drive = new localDrive(dir.c_str(), 512, 32, 32765, 16000, 0xF8);
		drive->label.SetLabel(label.c_str(), false, true);
		Drives[letter-'A'] = drive;
		mem_writeb(Real2Phys(dos.tables.mediaid) + (letter-'A') * 9, drive->GetMediaByte());
	}
}

/* SETUP, at the DOS prompt.  The screen is not DOS's and not drawn here:
 * the machine takes the tube back for as long as it is up, and this program
 * stands still meanwhile so that DOS is not doing anything behind it. */
static void DBP_DXMSetupProgram(Program** make)
{
	struct DXMSetup : Program
	{
		void Run(void)
		{
			if (!DXM_Present()) { WriteOut("SETUP needs the machine this DOS runs in.\n"); return; }
			bool up = true;
			if (!environ_cb(DXM_ENV_SETUP, &up)) return;
			for (Bit32u t0 = DBP_GetTicks(); !first_shell->exit;)
			{
				CALLBACK_Idle();
				up = false;
				if (!environ_cb(DXM_ENV_SETUP, &up) || !up) break;
				if ((DBP_GetTicks() - t0) > 3600000) break; /* an hour of it is enough */
			}
		}
	};
	*make = new DXMSetup;
}

/* CATALOG, at the DOS prompt.  Like SETUP the screen is the machine's; unlike
 * it, the program is asked to run things while the screen is up - a title,
 * its own setup, or a COMMAND in its directory - and the screen comes back
 * when they return.  A 1993 program shelled out the same way: it noted where
 * it was, ran the thing, and put the directory back. */
static void DBP_DXMCatalogProgram(Program** make)
{
	struct DXMCatalog : Program
	{
		void Excursion(const dxm_catalog_msg& m)
		{
			Bit8u drive0 = DOS_GetDefaultDrive();
			char dir0[DOS_PATHLENGTH + 2] = "\\";
			DOS_GetCurrentDir(0, dir0 + 1);
			if (m.drive >= 'A' && m.drive <= 'Z' && Drives[m.drive - 'A'])
				DOS_SetDrive((Bit8u)(m.drive - 'A'));
			if (m.dir[0] && !DOS_ChangeDir(m.dir))
				WriteOut("Cannot change to %s\n", m.dir);
			char line[128];
			if (m.kind == 1)
			{
				WriteOut("\nType EXIT to return to CATALOG.\n\n");
				safe_strncpy(line, "COMMAND", sizeof line);
			}
			else
				safe_strncpy(line, m.cmd, sizeof line);
			first_shell->ParseLine(line);
			DOS_SetDrive(drive0);
			DOS_ChangeDir(dir0);
		}

		void Run(void)
		{
			if (!DXM_Present()) { WriteOut("CATALOG needs the machine this DOS runs in.\n"); return; }
			dxm_catalog_msg m;
			memset(&m, 0, sizeof m);
			m.op = 1;
			if (!environ_cb(DXM_ENV_CATALOG, &m)) return;
			for (Bit32u t0 = DBP_GetTicks(); !first_shell->exit;)
			{
				CALLBACK_Idle();
				m.op = 0;
				if (!environ_cb(DXM_ENV_CATALOG, &m)) break;
				if (m.rescan >= 'A' && m.rescan <= 'Z' && Drives[m.rescan - 'A'])
					Drives[m.rescan - 'A']->EmptyCache(); /* something was installed behind DOS */
				if (m.reply == 1) break;
				if (m.reply == 2)
				{
					Excursion(m);
					m.op = 2;
					if (!environ_cb(DXM_ENV_CATALOG, &m)) break;
					t0 = DBP_GetTicks();
					continue;
				}
				if ((DBP_GetTicks() - t0) > 3600000) break; /* an hour of browsing is enough */
			}
		}
	};
	*make = new DXMCatalog;
}

static void DBP_DXMBiosProgram(Program** make)
{
	/* The box, in the 80-column screen: the rules and the rows are the
	 * same width, and every field starts in a fixed column.
	 *
	 * Double lines throughout, and not by taste: a national keyboard brings
	 * its code page with it, and CP850, CP860 and the rest spend several of
	 * CP437's box-drawing slots on accented capitals.  The double-line set
	 * (\xC9 \xCD \xBB \xBA \xCC \xB9 \xC8 \xBC) is the part they all keep;
	 * the single-into-double joints \xC7 and \xB6 are not, and came out as
	 * letters on a Portuguese machine. */
	enum { WIDTH = 79, LEFT = 1, RIGHT = 78, L_LABEL = 3, L_VALUE = 22, R_LABEL = 41, R_VALUE = 60 };

	struct DXMBios : Program
	{
		/* Wait, with the emulated machine still running: the pauses are
		 * the point of this program, and a busy loop would freeze DOS. */
		void Wait(Bit32u ms)
		{
			for (Bit32u t0 = DBP_GetTicks(); (DBP_GetTicks() - t0) < ms && !first_shell->exit;)
				CALLBACK_Idle();
		}

		/* The machine's drive, for as long as the next thing takes. */
		void Drive(Bit32u ms)
		{
			unsigned d = ms;
			environ_cb(DXM_ENV_DRIVE, &d);
			Wait(ms);
		}

		static void Put(char* row, int col, const char* s)
		{
			for (int i = 0; s[i] && col + i < RIGHT; i++) row[col + i] = s[i];
		}
		static void Blank(char* row)
		{
			memset(row, ' ', WIDTH);
			row[WIDTH] = '\0';
		}
		void Centred(const char* s)
		{
			char row[WIDTH + 1];
			Blank(row);
			Put(row, (80 - (int)strlen(s)) / 2, s);
			WriteOut("%s\n", row);
		}
		void Rule(char l, char fill, char r)
		{
			char row[WIDTH + 1];
			Blank(row);
			row[LEFT] = l;
			memset(row + LEFT + 1, fill, RIGHT - LEFT - 1);
			row[RIGHT] = r;
			WriteOut("%s\n", row);
		}
		void Line(const char* llabel, const char* lvalue, const char* rlabel, const char* rvalue)
		{
			char row[WIDTH + 1];
			Blank(row);
			row[LEFT] = row[RIGHT] = '\xBA';
			Put(row, L_LABEL, llabel);
			if (*llabel) Put(row, L_VALUE - 2, ": ");
			Put(row, L_VALUE, lvalue);
			Put(row, R_LABEL, rlabel);
			if (*rlabel) Put(row, R_VALUE - 2, ": ");
			Put(row, R_VALUE, rvalue);
			WriteOut("%s\n", row);
		}

		/* What the emulated PC actually is, where it can be asked; the
		 * rest is the fiction the case is dressed in. */
		void Audio(char* out, size_t n)
		{
			Section_prop* sb = static_cast<Section_prop*>(control->GetSection("sblaster"));
			const char* type = (sb ? sb->Get_string("sbtype") : "none");
			int t = (!strcmp(type, "sb16") ? 6 : !strcmp(type, "sbpro2") ? 4 : !strcmp(type, "sbpro1") ? 4 : !strcmp(type, "sb2") ? 3 : !strcmp(type, "sb1") ? 1 : 0);
			if (!t) { safe_strncpy(out, "None", n); return; }
			snprintf(out, n, "A%3X I%d D%d H%d T%d", (unsigned)(Bitu)sb->Get_hex("sbbase"),
				sb->Get_int("irq"), sb->Get_int("dma"), sb->Get_int("hdma"), t);
		}

		void Run(void)
		{
			if (!DXM_Present()) return;

			unsigned mhz = 66;
			environ_cb(DXM_ENV_MHZ, &mhz);
			Bitu total_kb = MEM_TotalPages() * 4;
			char clock[16], base[16], ext[16], audio[32];
			snprintf(clock, sizeof clock, "%uMHz", mhz);
			snprintf(base, sizeof base, "%uK", (unsigned)(total_kb > 640 ? 640 : total_kb));
			snprintf(ext, sizeof ext, "%uK", (unsigned)(total_kb > 1024 ? total_kb - 1024 : 0));
			Audio(audio, sizeof audio);

			/* The tube is still on the POST: clear this screen and hold it
			 * blank, so the handover lands on nothing and the box is
			 * printed where it can be seen.  A frontend that never says so
			 * (one that died) does not hang the boot for longer than this. */
			INT10_SetVideoMode(0x3);
			for (Bit32u t0 = DBP_GetTicks(); !first_shell->exit && (DBP_GetTicks() - t0) < 60000;)
			{
				bool shown = false;
				CALLBACK_Idle();
				if (environ_cb(DXM_ENV_SHOWN, &shown) && shown) break;
			}
			if (first_shell->exit) return;
			Wait(300); /* the moment a real one took to put its second screen up */

			Centred("DOS ex Machina");
			Centred("System Configurations");
			Rule('\xC9', '\xCD', '\xBB');
			Line("CPU Type", (mhz >= 100 ? "486DX4" : mhz >= 50 ? "486DX2" : "486DX"), "Base Memory", base);
			Line("Co-Processor", "Installed", "Extended Memory", ext);
			Line("CPU Clock", clock, "Cache Memory", "256K");
			Rule('\xCC', '\xCD', '\xB9');
			Line("Diskette Drive A", "1.44M, 3.5 in.", "Display Type", "EGA/VGA");
			Line("Diskette Drive B", "None", "Serial Port(s)", "3F8 2F8");
			Line("Pri. Master Disk", "LBA,Mode 4,540MB", "MIDI Port", "330");
			Line("Pri. Slave  Disk", "None", "Audio", audio);
			Rule('\xC8', '\xCD', '\xBC');

			Wait(500);
			Drive(700); /* the boot tries A: before C: */
			WriteOut("\nStarting DXM-DOS...\n");
			Wait(400);

			/* The drivers DOS loaded before it reached the prompt.  The
			 * memory test is the one that took a moment, so it takes one
			 * here: the line is printed, then finished.  Three lines and no
			 * more - the box, the greeting and the prompt fill the rest of
			 * the 25 rows, and a fourth would scroll the box off the top. */
			WriteOut("HIMEM is testing extended memory...");
			Wait(900);
			WriteOut("done.\n");
			Wait(250);
			WriteOut("DXM Mouse Driver, Version 8.20\n");
			Wait(20);
			/* the key itself in bright white: DOS's CON device reads the
			 * ANSI escapes, the way ANSI.SYS did */
			WriteOut("Mouse enabled: \033[1;37mCTRL+F10\033[0m to release/capture.\n\n");
			Wait(500);
		}
	};
	*make = new DXMBios;
}

#endif
