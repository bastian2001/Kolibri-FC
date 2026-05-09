/*
 * Copyright (c) 2026 Kolibri-FC contributors
 * 
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 * 
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

import { createRouter, createWebHistory } from "vue-router"
import Home from "@views/Home.vue"
import Blackbox from "@views/Blackbox.vue"
import GpsMag from "@views/GpsMag.vue"
import Motors from "@views/Motors.vue"
import Osd from "@views/Osd.vue"
import Receiver from "@views/Receiver.vue"
import Tasks from "@views/Tasks.vue"
import Tuning from "@views/Tuning.vue"
import Cli from "@views/Cli.vue"
import Sensors from "@views/Sensors.vue"
import Battery from "@views/Battery.vue"
import Vtx from "@views/Vtx.vue"
import HardwareSetup from "@views/HardwareSetup.vue"

export const routes = [
	{ path: "/", component: Home, name: "Home" },
	{ path: "/hardwaresetup", component: HardwareSetup, name: "Hardware Setup" },
	{ path: "/blackbox", component: Blackbox, name: "Blackbox" },
	{ path: "/tasks", component: Tasks, name: "Tasks" },
	{ path: "/osd", component: Osd, name: "OSD" },
	{ path: "/motors", component: Motors, name: "Motors" },
	{ path: "/battery", component: Battery, name: "Battery" },
	{ path: "/receiver", component: Receiver, name: "Receiver" },
	{ path: "/vtx", component: Vtx, name: "VTX" },
	{ path: "/sensors", component: Sensors, name: "Sensors" },
	{ path: "/tuning", component: Tuning, name: "Tuning" },
	{ path: "/gpsmag", component: GpsMag, name: "GPS & Mag" },
	{ path: "/cli", component: Cli, name: "CLI" },
]

const router = createRouter({
	history: createWebHistory(),
	routes,
})

export default router
