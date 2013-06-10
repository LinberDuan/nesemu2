/***************************************************************************
 *   Copyright (C) 2013 by James Holodnak                                  *
 *   jamesholodnak@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "emu/events.h"
#include "emu/emu.h"
#include "misc/log.h"
#include "misc/config.h"
#include "system/video.h"
#include "system/sound.h"
#include "nes/nes.h"
#include "misc/paths.h"
#include "nes/nes.h"
#include "nes/state/state.h"
#include "mappers/mapperid.h"

static void setfullscreen(int fs)
{
	video_kill();
	sound_pause();
	config_set_bool("video.fullscreen",fs);
	sound_play();
	video_init();
	ppu_sync();
}

int emu_event(int id,void *data)
{
	int ret = 0;
	char dest[1024];

	switch(id) {

		case E_QUIT:
			quit++;
			break;

		case E_LOADROM:
			if((ret = nes_load((char*)data)) == 0) {
				nes_reset(1);
				running = config_get_bool("video.pause_on_load") ? 0 : 1;
			}
			break;

		case E_UNLOAD:
			nes_unload();
			break;

		case E_SOFTRESET:
			nes_reset(0);
			break;

		case E_HARDRESET:
			nes_reset(1);
			break;

		case E_LOADSTATE:
			paths_makestatefilename(nes->romfilename,dest,1024);
			nes_loadstate(dest);
			break;

		case E_SAVESTATE:
			paths_makestatefilename(nes->romfilename,dest,1024);
			nes_savestate(dest);
			break;

		case E_FLIPDISK:
			if(nes->cart == 0)
				break;
			if((nes->cart->mapperid & B_TYPEMASK) == B_FDS) {
				u8 data[4] = {0,0,0,0};

				nes->mapper->state(CFG_SAVE,data);
				if(data[0] == 0xFF)
					data[0] = 0;
				else
					data[0] ^= 1;
				nes->mapper->state(CFG_LOAD,data);
				log_printf("disk inserted!  side = %d\n",data[0]);
			}
			else
				log_printf("cannot flip disk.  not fds.\n");
			break;

		case E_DUMPDISK:
			if(nes->cart && (nes->cart->mapperid & B_TYPEMASK) == B_FDS) {
				FILE *fp;

				log_printf("dumping disk as dump.fds\n");
				if((fp = fopen("dump.fds","wb")) != 0) {
					fwrite(nes->cart->disk.data,1,nes->cart->disk.size,fp);
					fclose(fp);
				}
			}
			break;

		case E_TOGGLERUNNING:
			running ^= 1;
			break;

		case E_PAUSE:
			running = 0;
			break;

		case E_UNPAUSE:
			running = 1;
			break;

		case E_TOGGLEFULLSCREEN:
			setfullscreen(config_get_bool("video.fullscreen") ^ 1);
			break;

		case E_FULLSCREEN:
			setfullscreen(1);
			break;

		case E_WINDOWED:
			setfullscreen(0);
			break;

		//unhandled event
		default:
			ret = -1;
			break;
	}
	return(ret);
}
