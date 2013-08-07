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
#include "DeferredMonitor.hxx"
#include "Loop.hxx"

void
DeferredMonitor::Cancel()
{
	const auto id = source_id.exchange(0);
	if (id != 0)
		g_source_remove(id);
}

void
DeferredMonitor::Schedule()
{
	const unsigned id = loop.AddIdle(Callback, this);
	const auto old_id = source_id.exchange(id);
	if (old_id != 0)
		g_source_remove(old_id);
}

void
DeferredMonitor::DoRun()
{
	const auto id = source_id.exchange(0);
	if (id != 0)
		Run();
}

gboolean
DeferredMonitor::Callback(gpointer data)
{
	DeferredMonitor &monitor = *(DeferredMonitor *)data;
	monitor.Run();
	return false;
}
